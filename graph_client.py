#! /usr/bin/env python3
import socket
import struct
import argparse
import logging
import sys
import os
import concurrent.futures



# host e porta per la comunicazione tra client e server 
HOST = "127.0.0.1"
PORT = 50000
ADDRESS = (HOST, PORT)



# configurazione del file di log per il debug
logging.basicConfig(filename='client.log',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')



# Funzione che crea i threads ausiliari. A ogni thread viene dato in
# input uno dei file passati al programma da linea di comando
def distribuisci_lavoro(files):
    numero_threads = len(files)

    # creazione threads
    with concurrent.futures.ProcessPoolExecutor(max_workers=numero_threads) as executor:
        for i in range(numero_threads):
            executor.submit(invia_grafo_server,files[i],ADDRESS)



# Funzione che ogni singolo thred esegue i seguenti compiti: 
# - legge il file passato come input ed elimina le linee di commneto;
# - si collega al server e invia i nodi del grafo;
# - attende la risposta del server e stampa a schermo le informazioni necessarie;
def invia_grafo_server(file, address):
    logging.debug(f"Creato thread: {os.getpid()}, con input: {file}")

    # Lettura del file
    with open(file, 'r') as f:
        lines = f.readlines()
        archi = []
        for l in lines:
            line = l.strip()
            if line.startswith('%'):
                continue
            archi.append(line)
        n_nodi, n_archi, n_coppie = map(int, archi[0].split())

        # Creazione della socket e connessione al server 
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as c:
            c.connect(address)
            c.sendall(struct.pack("!3i",n_nodi, n_archi, n_coppie ))
            logging.debug(f"Inviati al server n: {n_nodi}, a: {n_archi}, c: {n_coppie}")
            for line in archi[1:]:
                i, j = map(int, line.split())
                c.sendall(struct.pack('!2i', i, j))
            logging.debug(f"Inviati al server tutte le coppie di {archi[1:]}")

            # Attesa della risposta del server
            exit_code_data = recv_all(c, 4)
            exit_code = struct.unpack('!i', exit_code_data)[0]
            logging.debug(f"Ricevuto dal server l'Exit_code di pagerank: {exit_code}")

            # Stampa  del risultato di pagerank
            if exit_code == 0:
                lunghezza = recv_all(c,4)
                n = struct.unpack("!i",lunghezza)[0]            
                data = recv_all(c,4*n)
                res = data.decode("utf-8").split('\n')
                print(f"{file} Exit_code: {exit_code}")
                for line in res[:-1]:
                    print(f"{file} {line}")
                print(f"{file} Bye")
            else:
                lunghezza = recv_all(c, 4)
                n = struct.unpack("!i", lunghezza)[0]
                data = recv_all(c, 4*n)
                err = data.decode("utf-8")
                print(f"{file} Errore nel calcolo del PageRank, exit code {exit_code}:")
                print(f"{file} {err}")
            c.shutdown(socket.SHUT_RDWR)
            logging.debug(f"Terminazione thread-client: {os.getpid()}")



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



# Parsing degli argomenti da linea di comando e 
# prima chiamata alla funzione che distribuisce i 
# vari file tra i vari thread
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Uso: ./graph_client.py file_mtx_1 [file_mtx_2 ...]")
    parser.add_argument('infiles', metavar='file', type=str, nargs='+', help='file_mtx_1 [file_mtx_2 ...]')
    args = parser.parse_args()
    infiles = args.infiles
    distribuisci_lavoro(infiles)
