# COMPILAZIONE
Per compilare il programma pagerank.c eseguire il comando make da temrinale nella stessa directory del programma.
Per lanciare il server eseguire il comando ./graph_server.py.
Per lanciare il client eseguire il comando ./graph_client.py infile1 [infile...infileN].
L'eseguibile pagerank può essere invocato sia tramite client che direttamente da linea di comando, passandogli il file di input in formato mtx. 
# SERVER: 
Il server accetta connessioni TCP in un indirizzo IP e una porta specificati come costanti HOST e PORT, raggruppate nella costante ADDRESS. Il server utilizza la libreria di logging di Python per registrare gli eventi su un file di log chiamato 'server.log'.
La socket viene creata dalla funzione 'inizializza_server' che si mette in ascolto sull'host e la porta dichiarati nelle rispettive costanti tramite un ciclo infinito. In questa fase veine anche creato un pool di thread che si occuperanno della gestione delle varie connessioni che arriveranno. Questo ciclo infinito di gestione delle connessioni può essere interrotto tramite l'eccezione che viene generata da tastiera (KeyboardInterrupt), la quale prima di 'chiudere' il server attende che i vari client che erano in connessi finiscano la loro esecuzione, per poi stampare a schermo il messaggio 'Bye dal server'.
Le singole connessioni sono gestite dalla funzione 'gestisci_connessione' a cui vengono passati come parametri in input la connessione e l'indirizzo creati dalla socket.bind(). Come prima cosa il server si mette in attesa di 12 bytes (i primi tre numeri della prima riga di un file .mtx) che vengono successivamente interpretati e assegnati alle variabili n_nodi e n_archi, n_coppie. Tramite la libreria python tempfile, viene creato un file temporaneo .mtx su cui vengono successivamente scritti i tre valori appena letti. Inizia poi la ricezione degli archi da parte del server a gruppi di dieci alla volta. Il server si mette ogni iterazione di 8 bytes alla volta (due numeri, ovvero i due nodi che rappresentano ogni arco) e fa dei controlli di validità scartando gli archi non validi e aggiungendo quelli validi nell'array 'archi'. Una volta letti tutti gli archi vengono scritti sul file temporaneo prima i tre valori ricevuti all'inizio e successivamente, tramite un ciclo, tutti gli altri. Viene infine chiuso il file temporaneo.
Tramite la libreria python subprocess viene eseguito l'eseguibile 'pagerank', dando in input il file temporaneo, e catturando l'output del programma.
Tramite la funzione snedall() viene inviato l'exit code dell'esecuzione di pagerank al client e viene codificato l'outupt del programma in 'utf-8' a seconda del codice di uscita: se è '0' viene catturato lo stdout dell'esecuzione di pagerank, altrimenti lo stderr.
Viene successivamente inviata al client la lunghezza dell'output codificato che viene successivamente inviato. Infine vengono scritte sul file di log le seguenti informazioni: il numero dei nodi del grafo, il nome del file temporaneo, il numero di archi scartati, il numero di archi validi e l'exit code di pagerank.
Nel codice di graph_server.py è presente anche la funzione recv_all(conn, n) che si assicura di ricevere esattamente n byte, assicurando che non ci sono perdite di dati durante trasferimento.

# CLIENT:
Il client si connette tramite TCP a un indirizzo IP e una porta specificati come costanti HOST e PORT, raggruppate nella costante ADDRESS. Il client utilizza la libreria di logging di Python per registrare gli eventi su un file di log chiamato 'client.log'.
Il client utilizza la libreria argparse per analizzare gli argomenti della riga di comando. In caso non ci siano elementi sulla linea di comando stampa il messaggio di errore seguito dal modo di utilizzo del programma stesso. Accetta come input una lista di file .mtx i queli vengono inseriti nella variabile args, e passati come input alla funzione distribuisci_lavoro().
Tale funzione crea un pool di thread, a onguno dei quali passa come input la funzione da eseguire uno dei file passati da linea di comando e l'host e la porta per connettersi al server. La funzione che esegue la maggior parte del lavoro e la funzione 'invia_grafo_server'.
Essa prende in input uno dei file passati da linea di comando e l'indirizzo a cui connettersi. Come prima cosa apre il file, lo legge scartando le linee di commento e splittando ogni linea, inserendola nell'array archi. Associa poi alle variabili n_nodi, n_archi e n_coppie la prima riga del file e subito dopo crea la socket per la comunicazione che si connette tramite socket.connect() all'indirizzo dato. I primi dati che invia sono le tre varibaili appena create e subito dopo, tramite un ciclo sull'array di linee splittato ulteriormente per contenere ogni singolo numero, le coppie di numeri che rappresentano ognuna un arco del grafo. Si mette poin in attesa di ricevere l'exit code dell'esecuzione di pagerank dal server. Attende poi la lunghezza della risposta dell'output di pagerank e una volta ricevuto stampa il risultato come richiesto dalle specifiche del progetto.

# PAGERANK:
Il programma pagerank include il file 'xerrori.h' per la gestione degli errori usato a lezione.

 # strutture dati:
 - 'nodoRank': contiene due campi, il double rank che contiene il rank calcolato e l'intero nodo che contiene l'indice (ovvero il numero del nodo) dell'array della soluzione. Questa struttura dati è stata usata per ordinare l'array del risusltato calcolato da pagerank e restituire quindi i top_k nodi con rank più alto.
 - 'nodeList': contiene un intero node il quale contiene il numero del nodo, e un puntatore a nodeList che punta alla rappresentazione del nodo successivo nella lista. Utilizzata per la rappresentazione della lista g->in.
 - 'grafo': rappresenta la struttura dati grafo. 'inmap' è stato pensato come un'array di liste.
 - 'arco': rappresenta una coppia di numeri, è stato usato per il passaggio di informazioni nel buffer produttori-consumatori.
 - 'datiInputConsumatori': struct con i dati da passare come input ai vari thread consumatori per la creazione del grafo.
 - 'datiInputPagerank': struct per passare in input i dati ai vari thread addetti al calcolo effettivo del pagerank.
 - 'datiInputGestoreSegnali': struct per passare in input al thread che gestisce i segnali la variabile 'continua', la quale è utilizzata per la terminazione del thread.
 
 # funzioni:
 - 'compNodoRank': funzione di supporto per ordinare tramite qsort il risultato del calcolo di pagerank, così da poter restituire i top_k nodi.
 - 'inizializzaGrafo': funzione di supporto per la creazione della struttura dati grafo.
 - 'arcoVisto': funzione di supporto per capire se un arco è già stato inserito nel grafo.
 - 'aggiungiArco': funzione di supporto per aggiungere un arco al grafo.
 - 'freeGrafo': funzione di supporto per la deallocazione della memoria usata per memorizzare la struttura dati grafo.
 - 'gestoreSegnaliBody': funzione che esegue il thread dedicato allo gestione dei segnali.
 - 'consumatoreBody': funzione che eseguono i thread consumatori, i quali leggono gli archi dal buffer. il meccanismo di terminazione di questi thread consiste nell'invio da parte del thread principale dell'arco (-1,-1).
 - 'pagerankBody': funzione che esegue ogni thread per effettuare il calcolo del vettore pagerank
 -'pagerank': funzione effettivamente chiamata dal main per eseguire il calcolo. Tale funzione crea i vari thread addetti al calcolo e li attende, ritornando il risultato al main.
 - 'main': come prima cosa il main blocca tutti i segnali e crea un thread per la gestione del segnale SIGUSR1. Successivamente, tramite la funzione di libreria getopt(), gestisce le opzioni da linea di comando. inizia poi la prima scansione del file durante la quale scarta le linee di commento e legge la prima riga utile del file. Crea poi i vari thread consumatori passandogli tutti i dati utili. Inizia una nuova scansione del file durante la quale, tramite uno schema produttori consumatori, passa ai thread tramite il buffer, le varie coppie create come archi. Immette infine nel buffer gli archi (-1,-1) e si mette in attesa della terminazione dei consumatori.
 Stampa poi le infomrazioni utili e avvia la chiamata alla funzione pagerank, e una volta restituito il risultato ordina la soluzione e stampa i risultati di pagerank. Infine dealloca tutta la memoria ed invia al thread che gestisce i seganli un seganle SIGUSR2 per farlo terminare.