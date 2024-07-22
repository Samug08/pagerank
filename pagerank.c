#include "xerrori.h"


// Definizione macro e variabili usate durante la gestione dei segnali
#define QUI __LINE__,__FILE__
#define Buf_size 10
volatile bool PGR_INIZIATO = false;
volatile int miglior_nodo = -1;
volatile double miglior_rank = 0.0;
volatile int iterazione_corrente_seganle = 0;


// Struttura per tenere traccia dei rank e dei nodi
typedef struct{
    double rank;
    int nodo;
} nodoRank;


// Definizione del tipo nodeList per i nodi entranti (g->in[j])
typedef struct nodeList{
    int node;
    struct nodeList *next;
} nodeList;


// Definizione della struttura grafo da inizializzare e passare come dato in input ai vari thread
typedef struct{
    int n;
    int *out;
    nodeList **in;
    int *de_node;
    int len_de;
} grafo;


// Struttura arco da inserire nel buffer
typedef struct{
    int i;
    int j;
} arco;


// Struct dei dati da passare in input ai 
// threads addetti alla lettura e costruzione del grafo
typedef struct{
    int *archi_validi;
    int *archi_non_validi;
    grafo *grafo;
    arco *buffer; 
    int *consumer_index;
    pthread_mutex_t *mu;
    sem_t *sem_free_slots;
    sem_t *sem_data_items;
} datiInputConsumatori;


// struct dei dati da passare in input ai
// thread addetti al calcolo del pagerank
typedef struct{
    grafo *g;
    double *x;
    double *xnext;
    double d;
    double *eps;
    int iterazioni;
    int max_iter;
    pthread_mutex_t *mutex;
    double teleporting;
    int start;
    int end;
    pthread_barrier_t *barrier;
    double *e;
} datiInputPagerank;


// Funzione di confronto per qsort per ordinare la soluzione e stampare i migliori nodi
int compNodoRank(const void *a, const void *b) 
{
    nodoRank *nr1 = (nodoRank *)a;
    nodoRank *nr2 = (nodoRank *)b;
    if (nr1->rank < nr2->rank) return 1;
    if (nr1->rank > nr2->rank) return -1;
    return 0;
}


// Funzione per inizializzare la struttura dati grafo
grafo *inizializzaGrafo(int nodi)
{
    grafo* g = (grafo*) malloc(sizeof(grafo));
    if(g == NULL) xtermina("Errore inizializzazione grafo", QUI);
    g->n = nodi;
    g->out = (int*) calloc(nodi, sizeof(int));
    if(g->out == NULL) xtermina("Errore creazione array g->out", QUI);
    g->in = (nodeList**) malloc(nodi*sizeof(nodeList*));
    if(g->in == NULL) xtermina("Errore creazione lista g->in", QUI);
    g->de_node = (int*) malloc(nodi*sizeof(int));
    if(g->de_node == NULL) xtermina("Errore creazione dead-node array", QUI);
    for (int i=0;i<nodi;i++) {
        g->in[i] = NULL;
    }
    g->len_de = 0;

    return g;
}


// Funzione che indaga se un arco è già stato visto 
bool arcoVisto(grafo *g, int i, int j)
{
    nodeList *nodo_corrente = g->in[j];
    while(nodo_corrente != NULL){
        if(nodo_corrente->node == i){
            
            return true;
        }
        nodo_corrente = nodo_corrente->next;
    }
    return false;
}


// Funzione per aggiungere un arco al grafo
void aggiungiArco(grafo *g, int i, int j) 
{
    if (i<0 || j<0) { // Salto la condizione di terminazione dei thread
        return;
    }
    nodeList *nuovoNodo = malloc(sizeof(nodeList));
    if(nuovoNodo == NULL) xtermina("Errore creazione e aggiunta arco nuovo", QUI);
    nuovoNodo->node = i;
    nuovoNodo->next = g->in[j];
    g->in[j] = nuovoNodo;
    g->out[i]++;
}


// Funzione per liberare la memoria del grafo
void freeGrafo(grafo *g) 
{
    for(int i=0;i<g->n;i++) {
        nodeList *current = g->in[i];
        while (current != NULL) {
            nodeList *tmp = current;
            current = current->next;
            free(tmp);
        }
    }
    free(g->de_node);
    free(g->out);
    free(g->in);
    free(g);
}


// Funzione dei thread consumatori
void *consumatoreBody(void *arg)
{
    datiInputConsumatori *d = (datiInputConsumatori *)arg;
    pthread_mutex_t *m = d->mu; 
    arco a;
    do {
        xsem_wait(d->sem_data_items,QUI);
        xpthread_mutex_lock(m,QUI);
        a.i = d->buffer[*(d->consumer_index) % Buf_size].i; 
        a.j = d->buffer[*(d->consumer_index) % Buf_size].j;
        *(d->consumer_index) += 1;
        if (a.i != a.j && a.i >= 0 && a.j >= 0 && a.i < d->grafo->n && a.j < d->grafo->n) {
            if (!arcoVisto(d->grafo, a.i, a.j)) {
                aggiungiArco(d->grafo, a.i, a.j);
                (*(d->archi_validi))++;
            } else {
                (*(d->archi_non_validi))++;
            }
        } else {
            (*(d->archi_non_validi))++;
        }        
        xpthread_mutex_unlock(m,QUI);
        xsem_post(d->sem_free_slots,QUI);
    } while(a.i != -1 && a.j != -1);
    pthread_exit(NULL);
}


// Funzione che eseguono i thread addetti al
// calcolo del pagerank, ogni thread esegue 
// una parte del calcolo del pagerank
void *pagerankBody(void *arg) {
    datiInputPagerank *d = (datiInputPagerank *)arg;
    
    // Ciclo per il calcolo del pagerank
    while(*(d->e)>=*(d->eps) && d->iterazioni<d->max_iter) {
        // Inizializzo xnext ed e a 0
        for (int i=d->start;i<d->end;i++) {
            d->xnext[i] = 0.0;
        }

        // Caloclo St all'iteazione (t)
        double somma_de_rank;
        if(d->start==0){
            somma_de_rank = 0;
            for (int i=0;i<d->g->len_de;i++) {
                somma_de_rank += d->x[d->g->de_node[i]];
            }
        }

        // Calcolo effettivo de pagerank
        for (int i=d->start;i<d->end;i++) {
            nodeList *current = d->g->in[i];
            while (current != NULL) {
                d->xnext[i] += d->d * d->x[current->node]/d->g->out[current->node];
                current = current->next;
            }
            d->xnext[i] += d->teleporting + d->d*somma_de_rank/d->g->n;
        }

        // Calcolo errore locale
        double errore_locale = 0.0;
        for (int i=d->start;i<d->end;i++) {
            errore_locale += fabs(d->xnext[i] - d->x[i]);
        }

        // Sincronizzazione thread dopo il calcolo del pagerank
        pthread_barrier_wait(d->barrier);

        // Azzero l'errore dell'iterazione precedente
        // se sono il 'primo' thread, gli altri attendono 
        // l'azzeramento sulla barriera
        if(d->start == 0){
            *(d->e) = 0;
        }

        // sincronizzazione thread dopo che il primo 
        // thread ha azzerato l'errore
        pthread_barrier_wait(d->barrier);

        // Aggiorno l'errore totale e il numero di iterazioni
        xpthread_mutex_lock(d->mutex, QUI);
        *(d->e) += errore_locale;
        d->iterazioni++;
        xpthread_mutex_unlock(d->mutex, QUI);

        // Sincronizzazione thread dopo il calcolo dell'errore
        pthread_barrier_wait(d->barrier);

        // Swap di x e xnext
        double *temp = d->x;
        d->x = d->xnext;
        d->xnext = temp;

        // Sincronizzazione prima di iniziare una nuova iterazione
        pthread_barrier_wait(d->barrier);
    }
    pthread_exit(NULL);
}


double *pagerank(grafo *g, double d, double eps, int maxiter, int taux, int *numiter) 
{
    int n = g->n;
    double *x = malloc(n*sizeof(double));
    double *xnext = malloc(n*sizeof(double));
    double teleporting = (1-d)/n;
    double valore_iniziale = 1.0/n;
    double e = 1+eps;
    int num_iter = 0;
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, taux);
    pthread_t t[taux];
    datiInputPagerank a[taux];
    pthread_mutex_t mu_err = PTHREAD_MUTEX_INITIALIZER;

    // Inizializzo il vettore X(1) con i valori 1/n,
    // è l'array che conterrà la soluzione alla fine di pagerank
    for (int i=0;i<n;i++) {
        x[i] = valore_iniziale;
    }

    // Creazione e passaggio parametri ai vari thread
    int size = n/taux;
    for (int i=0;i<taux; i++) {
        a[i].g = g;
        a[i].x = x;
        a[i].xnext = xnext;
        a[i].d = d;
        a[i].max_iter = maxiter;
        a[i].iterazioni = num_iter;
        a[i].teleporting = teleporting;
        a[i].eps = &eps;
        a[i].mutex = &mu_err;
        a[i].start = i*size;
        a[i].end = (i == taux-1) ? n : (i+1)*size;
        a[i].barrier = &barrier;
        a[i].e = &e;
        xpthread_create(&t[i], NULL, &pagerankBody, &a[i], QUI);
    }

    // Attesa dei thread
    for (int i=0;i<taux;i++) {
        pthread_join(t[i], NULL);
    }

     
    // Passaggio per parametro del numero di iterazioni fatte,
    // libero memoria e restituisco il risultato   
    *numiter = a->iterazioni;
    pthread_barrier_destroy(&barrier);
    xpthread_mutex_destroy(&mu_err, QUI);
    free(xnext);

    return x;
}




// Main
int main(int argc, char *argv[])
{
    // Valori di default
    int opt, top_k = 3, max_num_iter = 100, num_thread = 3;
    double damping_factor = 0.9, max_err = 1.0e-7;

    // Variabili inizializzate per l'uso di getopt
    extern char *optarg;
    extern int optind;

    // Parse dell'input da linea di comando
    while((opt = getopt(argc, argv, "k:m:d:e:t:")) != -1){
        switch(opt){
            case 'k':
                top_k = atoi(optarg);
                if(top_k == 0) xtermina("Errore conversione -k", QUI);
                break;
            case 'm':
                max_num_iter = atoi(optarg);
                if(max_num_iter == 0) xtermina("Errore conversione -m", QUI);
                break;
            case 'd':
                damping_factor = atof(optarg);
                break;
            case 'e':   
                max_err = atof(optarg);
                break;
            case 't':
                num_thread = atoi(optarg);
                if(num_thread == 0) xtermina("Errore conversione -t", QUI);
                break;
            default:
                fprintf(stderr, "Uso: %s [-k K] [-m M] [-d D] [-e E] [-t T] infile\n", argv[0]);
                exit(1);
        }
    }
    if(optind >= argc){
        fprintf(stderr, "Uso: %s [-k K] [-m M] [-d D] [-e E] [-t T] infile\n", argv[0]);
        exit(1);
    }

    // Inizializzo la variabile per il file
    char *infile = argv[optind];

    // Lettura prima riga del file
    FILE *f = fopen(infile, "r");
    if(f == NULL) xtermina("Errore apertura file di input", QUI);
    char *line = NULL;
    size_t len = 0;
    int n = 0;
    int c = 0;
    int n_archi = 0;
    int primaRiga = 0;
    while(getline(&line, &len, f) != -1){
        if(line[0] == '%') continue;
        if(primaRiga == 0) {
            sscanf(line, "%d %d %d", &n, &c, &n_archi);
            primaRiga++;
        } else {
            primaRiga++;
        }
    }

    // Creazione dei thread e passaggio dei vari parametri
    int archi_scartati = 0;
    int archi_validi = 0;
    grafo *g = inizializzaGrafo(n);
    arco buffer[Buf_size];
    int cindex=0;
    pthread_mutex_t muc = PTHREAD_MUTEX_INITIALIZER;
    pthread_t t[num_thread];
    datiInputConsumatori a[num_thread];
    sem_t sem_free_slots, sem_data_items;
    xsem_init(&sem_free_slots,0,Buf_size,QUI);
    xsem_init(&sem_data_items,0,0,QUI);
    for(int i=0;i<num_thread;i++) {
        a[i].archi_validi = &archi_validi;
        a[i].archi_non_validi = &archi_scartati;
        a[i].grafo = g;
        a[i].buffer = buffer;
        a[i].consumer_index = &cindex;
        a[i].mu = &muc;
        a[i].sem_data_items = &sem_data_items;
        a[i].sem_free_slots = &sem_free_slots;
        xpthread_create(&t[i],NULL,&consumatoreBody,&a[i],QUI);
    }

    // Torna all'inizio del file per leggere i dati
    fseek(f, 0, SEEK_SET);
    
    // Salta i commenti iniziali 
    while(getline(&line, &len, f) != -1){
        if(line[0] == '%') continue;
        break;
    }    

    // Legge gli archi dal file e li immette nel buffer 
    int pindex = 0;
    do {
        if(primaRiga > 0){
            primaRiga = 0;
            continue;
        }
        int i, j;   
        sscanf(line, "%d %d", &i, &j);
        arco arco = {i-1, j-1};
        xsem_wait(&sem_free_slots,QUI);
        buffer[pindex % Buf_size].i = arco.i;
        buffer[pindex % Buf_size].j = arco.j;
        pindex++;
        xsem_post(&sem_data_items,QUI);
    } while(getline(&line, &len, f) != -1); 

    // Chiusura file di input 
    int e = fclose(f);
    if(e == EOF) xtermina("Errore chiusura file di input", QUI);

    // Meccanismo di terminazione dei threads
    for(int i=0;i<num_thread;i++) {
        xsem_wait(&sem_free_slots,QUI);
        buffer[pindex % Buf_size].i = -1;
        buffer[pindex % Buf_size].j = -1;
        pindex++;
        xsem_post(&sem_data_items,QUI);
    } 
    
    // Join dei thread 
    for(int i=0;i<num_thread;i++) {
        xpthread_join(t[i],NULL,QUI);
    }

    // Creazione dell'insieme dead-end
    // e calcolo del numero di nodi in questo insieme 
    int nodi_dead_end = 0;
    int lunghezza_de = 0;
    for (int i=0;i<n;i++) {
        if (g->out[i] == 0) {
            g->de_node[lunghezza_de] = i;
            lunghezza_de++;
            nodi_dead_end++;
        }
    }
    g->len_de = lunghezza_de;

    // Stampa dei risultati della creazione del grafo
    printf("Number of nodes: %d\n", n);
    printf("Number of dead-end nodes: %d\n", nodi_dead_end);
    printf("Number of valid arcs: %d\n", archi_validi);

    // Chiamata alla funzione pagerank che esegue il calcolo
    int iterazioni = 0;
    double *ranks = pagerank(g,damping_factor,max_err,max_num_iter,num_thread,&iterazioni);
    double somma_rank = 0.0;

    // Stampa del risultato di pagerank
    if(iterazioni >= max_num_iter){
        printf("Did not converged after %d iterations\n", iterazioni);
    } else{
        printf("Converged after %d iterations\n", iterazioni);
    }
    for(int i=0;i<n;i++){
        somma_rank += ranks[i];
    }
    printf("Sum of ranks: %f   (should be 1)\n", somma_rank);
    
    // Creazione di un array di nodoRank per ordinare i rank
    nodoRank *nodoRanks = malloc(n*sizeof(nodoRank));
    for (int i=0;i<n;i++) {
        nodoRanks[i].rank = ranks[i];
        nodoRanks[i].nodo = i;
    }

    // Ordinamento dei nodoRanks in ordine decrescente
    qsort(nodoRanks, n, sizeof(nodoRank), compNodoRank);

    // Stampa dei top_k nodi con i rank più alti
    for (int i=0;i<top_k;i++) {
        printf("   %d %6f\n", nodoRanks[i].nodo, nodoRanks[i].rank);
    }

    // Deallocazione della memoria
    free(line);
    free(ranks);
    free(nodoRanks);
    xsem_destroy(&sem_free_slots, QUI);
    xsem_destroy(&sem_data_items, QUI);
    xpthread_mutex_destroy(&muc, QUI);
    freeGrafo(g);

    return 0;
}
