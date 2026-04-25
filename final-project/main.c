/*
 * La Casa de Hojas — Crucigrama Cambiante
 *
 *   - fork() / waitpid()  : cada vez que el tablero cambia, el padre hace fork().
 *                           El hijo imprime el tablero y muere. El padre lo recoge.
 *   - Hilo 1 (cambios)    : usa alarm() + pause() para esperar SIGALRM; cuando llega
 *                           cambia una palabra no adivinada y notifica al padre con SIGUSR1.
 *   - Hilo 2 (input)      : loop leyendo stdin, valida respuestas, notifica con SIGUSR1.
 *   - alarm()             : dentro del hilo de cambios, dispara SIGALRM cada N segundos.
 *   - SIGALRM / SIGUSR1   : SIGALRM despierta al hilo de cambios;
 *                           SIGUSR1 le dice al padre que haga fork() y renderice.
 *   - Mutex               : protege el dashboard y las palabras activas (accedidos
 *                           por ambos hilos al mismo tiempo).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>

/* ══════════════════════════════════════════
   CONSTANTES
   ══════════════════════════════════════════ */

#define NUM_ROWS         17
#define NUM_COLUMNS      10

#define NUM_WORDS         5   /* variantes por slot */

#define MAX_LEN_WORD     10
#define MAX_LEN_DESC    100

#define CHANGE_INTERVAL  20   /* segundos entre cambios */


/* ══════════════════════════════════════════
   STRUCT PALABRA
   ══════════════════════════════════════════ */

typedef struct {
    char word[MAX_LEN_WORD];
    char description[MAX_LEN_DESC];
} Word;

/* H1: fila 4, cols 5-8 (4 letras) */
Word horizontal1[NUM_WORDS] = {
    {"pato", "Ave acuatica de pico ancho que nada en lagunas."},
    {"gato", "Animal domestico de cuatro patas que maulle."},
    {"dato", "Informacion o hecho concreto que se conoce."},
    {"mato", "Arbusto pequeno que crece en zonas secas."},
    {"rato", "Periodo corto de tiempo; tambien un roedor."}
};

/* H2: fila 6, cols 2-6 (5 letras) */
Word horizontal2[NUM_WORDS] = {
    {"pacto", "Acuerdo formal entre dos o mas personas."},
    {"plato", "Utensilio redondo donde se sirve la comida."},
    {"pinto", "De color moteado o con manchas de varios colores."},
    {"pasto", "Hierba del campo que sirve de alimento al ganado."},
    {"punto", "Signo de puntuacion; tambien un lugar exacto."}
};

/* H3: fila 11, cols 1-5 (5 letras) */
Word horizontal3[NUM_WORDS] = {
    {"bolso", "Bolsa pequena que se lleva colgada para guardar objetos."},
    {"torso", "Parte del cuerpo humano sin cabeza ni extremidades."},
    {"gordo", "Persona o cosa que tiene mucha grasa o volumen."},
    {"sordo", "Persona que no puede escuchar o escucha muy poco."},
    {"polvo", "Particulas muy finas que flotan en el aire o en superficies."}
};

/* V1: col 6, filas 1-6 (6 letras) */
Word vertical1[NUM_WORDS] = {
    {"ganado", "Conjunto de animales de campo como vacas u ovejas."},
    {"pasado", "Tiempo que ya ocurrio antes del momento presente."},
    {"casado", "Persona que tiene matrimonio con otra."},
    {"lavado", "Accion de limpiar algo con agua y jabon."},
    {"pesado", "Objeto de gran peso o persona que resulta cansada."}
};

/* V2: col 2, filas 6-11 (6 letras) */
Word vertical2[NUM_WORDS] = {
    {"pajaro", "Animal vertebrado con alas y plumas que puede volar."},
    {"panico", "Miedo intenso y repentino que paraliza a las personas."},
    {"paramo", "Terreno yermo y frio ubicado a gran altitud."},
    {"pedazo", "Trozo o fragmento que se separa de algo mayor."},
    {"pelado", "Sin pelo; tambien un lugar sin vegetacion."}
};

/* V3: col 5, filas 10-15 (6 letras) */
Word vertical3[NUM_WORDS] = {
    {"comino", "Semilla aromatica usada como condimento en la cocina."},
    {"conejo", "Animal pequeno de orejas largas que salta y corre."},
    {"modelo", "Representacion de algo; persona que exhibe ropa."},
    {"bolero", "Musica y baile de origen espanol o cubano."},
    {"molino", "Maquina que muele granos usando el viento o el agua."}
};

/* ══════════════════════════════════════════
   ESTADO GLOBAL (protegido por mutex)
   ══════════════════════════════════════════ */

char dashboard[NUM_ROWS][NUM_COLUMNS];

char word_h1[5];   /* 4 letras + '\0' */
char word_h2[6];   /* 5 letras + '\0' */
char word_h3[6];   /* 5 letras + '\0' */
char word_v1[7];   /* 6 letras + '\0' */
char word_v2[7];   /* 6 letras + '\0' */
char word_v3[7];   /* 6 letras + '\0' */

int idx_h1, idx_h2, idx_h3, idx_v1, idx_v2, idx_v3;

int solved_h1, solved_h2, solved_h3;
int solved_v1, solved_v2, solved_v3;

int total_solved = 0;
int game_over    = 0;
char feedback[128];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pid_t parent_pid;

/* ══════════════════════════════════════════
   HELPERS DE POSICIÓN
   ══════════════════════════════════════════ */

/* Devuelve 1 si la celda (row,col) pertenece a la palabra indicada */
static int in_h1(int r, int c) { return r == 4  && c >= 5 && c <= 8;  }
static int in_h2(int r, int c) { return r == 6  && c >= 2 && c <= 6;  }
static int in_h3(int r, int c) { return r == 11 && c >= 1 && c <= 5;  }
static int in_v1(int r, int c) { return c == 6  && r >= 1 && r <= 6;  }
static int in_v2(int r, int c) { return c == 2  && r >= 6 && r <= 11; }
static int in_v3(int r, int c) { return c == 5  && r >= 10&& r <= 15; }

/* Devuelve 1 si la celda es intersección de dos palabras */
static int is_intersection(int r, int c) {
    int count = in_h1(r,c) + in_h2(r,c) + in_h3(r,c)
              + in_v1(r,c) + in_v2(r,c) + in_v3(r,c);
    return count >= 2;
}


/* ══════════════════════════════════════════
   COLOCAR PALABRAS EN EL TABLERO INTERNO
   (siempre guarda las letras reales; la
    lógica de ocultación está en print_board)
   ══════════════════════════════════════════ */

static void place_words_on_dashboard(void) {
    for (int i = 0; i < NUM_ROWS; i++)
        for (int j = 0; j < NUM_COLUMNS; j++)
            dashboard[i][j] = ' ';

    /* H1: fila 4, cols 5-8 */
    for (int j = 0; j < 4; j++)
        dashboard[4][5 + j] = word_h1[j];

    /* H2: fila 6, cols 2-6 */
    for (int j = 0; j < 5; j++)
        dashboard[6][2 + j] = word_h2[j];

    /* H3: fila 11, cols 1-5 */
    for (int j = 0; j < 5; j++)
        dashboard[11][1 + j] = word_h3[j];

    /* V1: col 6, filas 1-6 */
    for (int i = 0; i < 6; i++)
        dashboard[1 + i][6] = word_v1[i];

    /* V2: col 2, filas 6-11 */
    for (int i = 0; i < 6; i++)
        dashboard[6 + i][2] = word_v2[i];

    /* V3: col 5, filas 10-15 */
    for (int i = 0; i < 6; i++)
        dashboard[10 + i][5] = word_v3[i];
}

/* ══════════════════════════════════════════
   INICIALIZAR JUEGO
   ══════════════════════════════════════════ */

static void init_game(void) {
    srand((unsigned)time(NULL));

    idx_h1 = rand() % NUM_WORDS;
    idx_h2 = rand() % NUM_WORDS;
    idx_h3 = rand() % NUM_WORDS;
    idx_v1 = rand() % NUM_WORDS;
    idx_v2 = rand() % NUM_WORDS;
    idx_v3 = rand() % NUM_WORDS;

    strcpy(word_h1, horizontal1[idx_h1].word);
    strcpy(word_h2, horizontal2[idx_h2].word);
    strcpy(word_h3, horizontal3[idx_h3].word);
    strcpy(word_v1, vertical1[idx_v1].word);
    strcpy(word_v2, vertical2[idx_v2].word);
    strcpy(word_v3, vertical3[idx_v3].word);

    solved_h1 = solved_h2 = solved_h3 = 0;
    solved_v1 = solved_v2 = solved_v3 = 0;
    total_solved = 0;
    game_over    = 0;
    strcpy(feedback, "La casa ha despertado. Adivina sus secretos.");

    place_words_on_dashboard();
}

/* ══════════════════════════════════════════
   IMPRIMIR TABLERO
   (ejecutado exclusivamente en el hijo fork)

   Reglas de visualización por celda:
     · Vacío (' ')  → punto gris
     · Intersección no revelada → '*' amarillo
     · Celda de palabra resuelta → letra verde
     · Celda de palabra no resuelta:
         - muestra el número de pista en gris
   ══════════════════════════════════════════ */

static void print_board(void) {
    printf("\033[2J\033[H");

    printf("\033[33;1m"
           "\n  ╔════════════════════════════════════╗\n"
           "  ║       LA CASA DE HOJAS             ║\n"
           "  ║       Crucigrama Cambiante         ║\n"
           "  ╚════════════════════════════════════╝\n"
           "\033[0m\n");

    for (int i = 0; i < NUM_ROWS; i++) {
        printf("  ");
        for (int j = 0; j < NUM_COLUMNS; j++) {
            char ch = dashboard[i][j];

            /* Celda vacía */
            if (ch == ' ') {
                printf("\033[90m . \033[0m");
                continue;
            }

            /* Determinar a qué palabras pertenece esta celda */
            int belongs_h1 = in_h1(i,j), belongs_h2 = in_h2(i,j), belongs_h3 = in_h3(i,j);
            int belongs_v1 = in_v1(i,j), belongs_v2 = in_v2(i,j), belongs_v3 = in_v3(i,j);

            /* ¿Está resuelta alguna de las palabras propietarias? */
            int revealed = (belongs_h1 && solved_h1)
                        || (belongs_h2 && solved_h2)
                        || (belongs_h3 && solved_h3)
                        || (belongs_v1 && solved_v1)
                        || (belongs_v2 && solved_v2)
                        || (belongs_v3 && solved_v3);

            if (revealed) {
                /* Mostrar letra real en verde */
                printf("\033[32;1m %c \033[0m", (char)toupper((unsigned char)ch));
                continue;
            }

            /* Celda no revelada: ¿intersección? */
            if (is_intersection(i,j)) {
                printf("\033[33;1m * \033[0m");
                continue;
            }

            /* Celda de una sola palabra no resuelta: mostrar número de pista */
            int pista = 0;
            if (belongs_h1) pista = 1;
            else if (belongs_h2) pista = 2;
            else if (belongs_h3) pista = 3;
            else if (belongs_v1) pista = 4;
            else if (belongs_v2) pista = 5;
            else if (belongs_v3) pista = 6;

            printf("\033[90m %d \033[0m", pista);
        }
        printf("\n");
    }

    printf("\n\033[36;1m  ── HORIZONTALES ─────────────────────────────────────────\033[0m\n");
    printf(solved_h1 ? "  \033[32m1. [RESUELTA]\033[0m\n"
                     : "  \033[37m1. %s\033[0m\n", horizontal1[idx_h1].description);
    printf(solved_h2 ? "  \033[32m2. [RESUELTA]\033[0m\n"
                     : "  \033[37m2. %s\033[0m\n", horizontal2[idx_h2].description);
    printf(solved_h3 ? "  \033[32m3. [RESUELTA]\033[0m\n"
                     : "  \033[37m3. %s\033[0m\n", horizontal3[idx_h3].description);

    printf("\033[34;1m  ── VERTICALES ───────────────────────────────────────────\033[0m\n");
    printf(solved_v1 ? "  \033[32m4. [RESUELTA]\033[0m\n"
                     : "  \033[37m4. %s\033[0m\n", vertical1[idx_v1].description);
    printf(solved_v2 ? "  \033[32m5. [RESUELTA]\033[0m\n"
                     : "  \033[37m5. %s\033[0m\n", vertical2[idx_v2].description);
    printf(solved_v3 ? "  \033[32m6. [RESUELTA]\033[0m\n"
                     : "  \033[37m6. %s\033[0m\n", vertical3[idx_v3].description);

    printf("\n  \033[35;1m-> %s\033[0m\n", feedback);
    printf("  \033[90mResueltas: %d/6\033[0m\n", total_solved);

    if (!game_over)
        printf("\033[33m\n  Numero de pregunta (0 = salir): \033[0m");
    fflush(stdout);
}

/* ══════════════════════════════════════════
   RENDER: padre hace fork, hijo imprime,
           padre recoge con waitpid
   ══════════════════════════════════════════ */

static volatile sig_atomic_t render_flag = 0;

static void handler_sigusr1(int sig) {
    (void)sig;
    render_flag = 1;
}

/* ══════════════════════════════════════════
   FORK — Se crea hijo cada vez que hay que imprimir el tablero
   ══════════════════════════════════════════ */
static void do_render(void) {
    render_flag = 0;
    pid_t child = fork();
    if (child < 0) { perror("fork"); return; }
    if (child == 0) {
        print_board();
        exit(0);
    }
    waitpid(child, NULL, 0);
}

/* ══════════════════════════════════════════
   HILO 1 — GESTOR DE CAMBIOS
   Usa alarm() + pause() para esperar SIGALRM.
   ══════════════════════════════════════════ */

static volatile sig_atomic_t alarm_flag = 0;

static void handler_sigalrm(int sig) {
    (void)sig;
    alarm_flag = 1;
}

static void *thread_cambios(void *arg) {
    (void)arg;

    signal(SIGALRM, handler_sigalrm);
    alarm(CHANGE_INTERVAL);

    while (1) {
        pause();   /* duerme hasta cualquier señal */

        if (!alarm_flag) continue;
        alarm_flag = 0;

        pthread_mutex_lock(&mutex);

        if (game_over) { pthread_mutex_unlock(&mutex); break; }

        /* Slots disponibles para cambiar */
        int slots[6], n = 0;
        if (!solved_h1) 
            slots[n++] = 1;
        if (!solved_h2) 
            slots[n++] = 2;
        if (!solved_h3) 
            slots[n++] = 3;
        if (!solved_v1) 
            slots[n++] = 4;
        if (!solved_v2) 
            slots[n++] = 5;
        if (!solved_v3) 
            slots[n++] = 6;

        if (n > 0) {
            int slot = slots[rand() % n];
            int next;
            switch (slot) {
                case 1:
                    next = (idx_h1 + 1) % NUM_WORDS;  idx_h1 = next;
                    strcpy(word_h1, horizontal1[next].word);
                    snprintf(feedback, 127, "*** La casa muta *** Pista 1 ha cambiado.");
                    break;
                case 2:
                    next = (idx_h2 + 1) % NUM_WORDS;  idx_h2 = next;
                    strcpy(word_h2, horizontal2[next].word);
                    snprintf(feedback, 127, "*** La casa muta *** Pista 2 ha cambiado.");
                    break;
                case 3:
                    next = (idx_h3 + 1) % NUM_WORDS;  
                    idx_h3 = next;
                    strcpy(word_h3, horizontal3[next].word);
                    snprintf(feedback, 127, "*** La casa muta *** Pista 3 ha cambiado.");
                    break;
                case 4:
                    next = (idx_v1 + 1) % NUM_WORDS;  
                    idx_v1 = next;
                    strcpy(word_v1, vertical1[next].word);
                    snprintf(feedback, 127, "*** La casa muta *** Pista 4 ha cambiado.");
                    break;
                case 5:
                    next = (idx_v2 + 1) % NUM_WORDS;  
                    idx_v2 = next;
                    strcpy(word_v2, vertical2[next].word);
                    snprintf(feedback, 127, "*** La casa muta *** Pista 5 ha cambiado.");
                    break;
                case 6:
                    next = (idx_v3 + 1) % NUM_WORDS;  idx_v3 = next;
                    strcpy(word_v3, vertical3[next].word);
                    snprintf(feedback, 127, "*** La casa muta *** Pista 6 ha cambiado.");
                    break;
            }
            place_words_on_dashboard();
        }

        pthread_mutex_unlock(&mutex);

        kill(parent_pid, SIGUSR1);   /* pedir render al padre */
        alarm(CHANGE_INTERVAL);       /* re-armar alarma */
    }
    return NULL;
}

/* ══════════════════════════════════════════
   HILO 2 — GESTOR DE INPUT
   ══════════════════════════════════════════ */

static void str_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

static void *thread_input(void *arg) {
    (void)arg;

    char num_buf[8];
    char ans_buf[MAX_LEN_WORD + 4];

    while (1) {
        if (!fgets(num_buf, sizeof(num_buf), stdin)) break;
        int n = atoi(num_buf);

        if (n == 0) {
            pthread_mutex_lock(&mutex);
            game_over = 1;
            strcpy(feedback, "El jugador abandona la casa...");
            pthread_mutex_unlock(&mutex);
            kill(parent_pid, SIGUSR1);
            break;
        }

        if (n < 1 || n > 6) {
            pthread_mutex_lock(&mutex);
            snprintf(feedback, 127, "Numero invalido. Elige entre 1 y 6.");
            pthread_mutex_unlock(&mutex);
            kill(parent_pid, SIGUSR1);
            continue;
        }

        /* ¿Ya resuelta? */
        int already = (n==1 && solved_h1) || (n==2 && solved_h2) || (n==3 && solved_h3)
                   || (n==4 && solved_v1) || (n==5 && solved_v2) || (n==6 && solved_v3);

        if (already) {
            snprintf(feedback, 127, "La palabra %d ya fue resuelta.", n);
            pthread_mutex_unlock(&mutex);
            kill(parent_pid, SIGUSR1);
            continue;
        }

        printf("\033[33m  Respuesta: \033[0m");
        fflush(stdout);

        if (!fgets(ans_buf, sizeof(ans_buf), stdin)) break;
        ans_buf[strcspn(ans_buf, "\n\r")] = '\0';
        str_upper(ans_buf);

        pthread_mutex_lock(&mutex);
        if (game_over) { pthread_mutex_unlock(&mutex); break; }

        

        /* Palabra correcta actual */
        char correct[MAX_LEN_WORD];
        switch (n) {
            case 1: strcpy(correct, word_h1); break;
            case 2: strcpy(correct, word_h2); break;
            case 3: strcpy(correct, word_h3); break;
            case 4: strcpy(correct, word_v1); break;
            case 5: strcpy(correct, word_v2); break;
            case 6: strcpy(correct, word_v3); break;
            default: correct[0] = '\0';
        }
        str_upper(correct);

        if (strcmp(ans_buf, correct) == 0) {
            switch (n) {
                case 1: solved_h1 = 1; break;
                case 2: solved_h2 = 1; break;
                case 3: solved_h3 = 1; break;
                case 4: solved_v1 = 1; break;
                case 5: solved_v2 = 1; break;
                case 6: solved_v3 = 1; break;
            }
            total_solved++;

            if (total_solved == 6) {
                game_over = 1;
                strcpy(feedback, "¡¡FELICIDADES!! Has descifrado todos los secretos de la casa.");
            } else {
                snprintf(feedback, 127, "Correcto! Pista %d resuelta.", n);
            }
        } else {
            snprintf(feedback, 127, "Incorrecto para pista %d. Sigue intentando.", n);
        }

        pthread_mutex_unlock(&mutex);
        kill(parent_pid, SIGUSR1);

        if (game_over) break;
    }
    return NULL;
}

/* ══════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════ */

int main(void) {
    parent_pid = getpid();

    /* SIGUSR1 despierta al padre para hacer fork+render */
    signal(SIGUSR1, handler_sigusr1);

    init_game();
    do_render();   /* render inicial */

    pthread_t tid_cambios, tid_input;
    pthread_create(&tid_cambios, NULL, thread_cambios, NULL);
    pthread_create(&tid_input,   NULL, thread_input,   NULL);

    /*
     * El padre espera SIGUSR1 con pause().
     * Cuando llega, render_flag queda en 1 y hacemos fork+render.
     */
    while (1) {
        pause();
        if (render_flag) do_render();

        pthread_mutex_lock(&mutex);
        int over = game_over;
        pthread_mutex_unlock(&mutex);
        if (over) break;
    }

    pthread_cancel(tid_cambios);
    pthread_cancel(tid_input);
    pthread_join(tid_cambios, NULL);
    pthread_join(tid_input,   NULL);
    alarm(0);

    printf("\n\033[36m  Gracias por jugar La Casa de Hojas.\033[0m\n\n");
    return 0;
}