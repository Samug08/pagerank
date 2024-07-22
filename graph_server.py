#! /usr/bin/env python3
import socket
import threading
import tempfile
import subprocess
import logging
import struct
import concurrent.futures



# Host e porta per la comunicazione tra client e server 
HOST = "127.0.0.1"
PORT = 50000
ADDRESS = (HOST, PORT)



# Configurazione del file di log
logging.basicConfig(filename='server.log',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')



# Funzione che esegue ogni singolo thread del pool e gestisce
# la connessione con il singolo client
def gestisci_connessione(conn, addr):
    with conn:
        # Messaggio di conferma connessione
        print(f"{threading.current_thread().name} contattato da {addr}")

        # Lettura del numero di nodi, archi e coppie
        data = recv_all(conn, 12) 
        n_nodi, n_archi, n_coppie = struct.unpack('!3i', data)
        logging.debug(f"Ricevuti dal client: n: {n_nodi}, a: {n_archi}, c: {n_coppie}")

        # Creazione del file temporaneo
        temp_file = tempfile.NamedTemporaryFile(delete=False, mode='w', suffix='.mtx')
        temp_file_name = temp_file.name

        # Leggo archi a gruppi di 10 alla volta,
        # tengo quelli validi e scarto archi non validi 
        archi = []
        archi_validi = 0
        archi_scartati = 0
        divisione = n_coppie//10
        resto = n_coppie%10
        for _ in range(divisione):
            for _ in range(10):
                data = recv_all(conn, 8)
                i, j = struct.unpack('!2i', data)
                if 1 <= i <= n_nodi and 1 <= j <= n_archi:
                    archi.append((i,j))
                    logging.debug(f"Aggiunti: {i,j}")
                    archi_validi += 1
                else:
                    logging.debug(f"Scartati: {i,j}")
                    archi_scartati += 1 
        if resto != 0:
            for _ in range(resto):
                data = recv_all(conn, 8)
                i, j = struct.unpack('!2i', data)
                if 1 <= i <= n_nodi and 1 <= j <= n_archi:
                    archi.append((i,j))
                    logging.debug(f"Aggiunti: {i,j}")
                    archi_validi += 1
                else:
                    logging.debug(f"Scartati: {i,j}")
                    archi_scartati += 1  
        
        # Aggiorno il valore degli numero di coppie di archi
        # e scrivo tutti i dati sul file temporaneo
        n_coppie -= archi_scartati
        temp_file.write(f"{n_nodi} {n_archi} {n_coppie}\n")
        logging.debug(f"Scritti su temp_file: n: {n_nodi}, a: {n_archi}, c: {n_coppie}")
        for i, j in archi:
            temp_file.write(f"{i} {j}\n")
        temp_file.close()

        # Esecuzione del programma pagerank
        result = subprocess.run(['./pagerank', temp_file_name], capture_output=True, text=True)

        # Invio exit code
        conn.sendall(struct.pack("!i", result.returncode))

        # Prepara i dati da inviare al client
        if result.returncode == 0:
            output_data = result.stdout.encode("utf-8")                
        else:
            output_data = result.stderr.encode("utf-8")
        
        # Invio della lunghezza dei dati
        conn.sendall(struct.pack('!i', len(output_data)))

        # Invio dei dati
        for d in output_data:
            conn.sendall(struct.pack("!i", d))

        # Logging delle informazioni
        logging.debug(f"Grafo con {n_nodi} nodi, file temporaneo: {temp_file_name}, "
                    f"archi scartati: {archi_scartati}, archi validi: {archi_validi}, "
                    f"exit code: {result.returncode}")



# Funzione che riceve esattamente n byte dal socket
# e li restituisce come una sequenza di byte
def recv_all(conn,n):
    chunks = b''
    bytes_recd = 0
    while bytes_recd < n:
        chunk = conn.recv(min(n - bytes_recd, 1024))
        if len(chunk) == 0:
            raise RuntimeError("socket connection broken")
        chunks += chunk
        bytes_recd = bytes_recd + len(chunk)
    return chunks



# Funzione che inizializza il server e gestisce le connessioni tramite un pool di threads
def inizializza_server(host=HOST,port=PORT):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:  
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            
            s.bind((host, port))
            s.listen()
            with concurrent.futures.ThreadPoolExecutor() as executor:
                while True:
                    print("In attesa di un client...")
                    conn, addr = s.accept()
                    executor.submit(gestisci_connessione, conn,addr)
        except KeyboardInterrupt:
            pass
        s.shutdown(socket.SHUT_RDWR)
        print('Bye dal server')



if __name__ == "__main__":
    inizializza_server()
