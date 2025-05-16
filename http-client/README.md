# Tema 4 - Client HTTP

## Descriere

Clientul HTTP pe care l-am implementat in aceasta tema este alcatuit din 2 componente:

- Biblioteca HTTP (`http/`): aceasta contine implementarea unei biblioteci HTTP folosita pentru comunicarea cu serverul.
- Interfata de linie de comanda (`cli.hpp/cli.cpp`): aceasta contine implementarea interfetei cli care primeste comenzile utilizatorului si realizeaza apelurile necesare catre biblioteca HTTP.

### Biblioteca HTTP

Aceasta este o biblioteca minimalista care foloseste protocolul `HTTP/1.1` pentru a comunica cu serverul. Folosirea bibliotecii consta in instantierea unui obiect `http::Client` si apelarea metodelor corespunzatoare tipului de request dorit. Tipurile de request-uri suportate sunt: **GET**, **POST**, **PUT**, **DELETE**. Fiecare metoda primeste ca parametru un URL path si, in functie de caz, un payload si o serie de headere, sau direct un obiect de tip `http:Request`.

Gestionarea conexiunii este realizata in interiorul clasei `http::Client`. Conexiunea nu se realizeaza la instantiere, ci intr-un mod "lenes" atunci cand este necesar. In momentul in care se face request-ul, se verifica daca conexiunea este deja deschisa, in caz contrar se incearca realizarea conexiunii cu serverul. Astfel, clientul HTTP va incerca sa mentina o legatura persistenta cu serverul pentru a evita overhead-ul care vine cu redeschiderea conexiunii de fiecare data. In acest scop, am inclus si un header `Connection: keep-alive` in request-uri, desi conform specificatiei `HTTP/1.1` conexiunea ar trebui sa fie persistenta in mod implicit.

Partea de networking este realizata folosind sockets POSIX (`sockets.h`), iar citirile si scrierile sunt realizate folosind apelurile blocante `read()` si `write()`. De asemenea, socket-ul este configurat sa aiba un timeout atat pe citire cat si pe scriere, pentru a evita blocarea in cazul in care serverul nu raspunde sau in cazul in care conexiunea s-a pierdut.

Parsarea liniei de status si a headerelor din raspuns este realizata folosind pattern-uri regex, din ratiuni de simplitate si eficienta (folosirea de regex-uri compile time).

### Interfata de linie de comanda

Interfata de linie de comanda are urmatorul flux:

1. Se citeste o comanda de la utilizator
2. Se apeleaza handler-ul corespunzator comenzii, sau se afiseaza un mesaj de eroare in cazul in care comanda nu este valida
3. Handler-ul va citi si parsa toate argumentele necesare, asigurand validarea acestora. In cazul in care unul dintre argumente este invalid, se arunca o exceptie, care este prinsa in functia principala (`Cli::run()`) si se afiseaza un mesaj de eroare corespunzator.
4. Se apeleaza metoda corespunzatoare din biblioteca HTTP, cu headerele si payload-ul corespunzator. Daca request-ul a fost realizat cu succes, se afiseaza `SUCCESS: <mesaj>`, urmat, optional, de payload-ul raspunsului. In cazul in care request-ul a esuat sau raspunsul are un cod de eroare, se afiseaza `ERROR: <mesaj_eroare>`. De precizat faptul ca request-urile se realizeaza cu reincercari automate (in numar de 3), pentru a evita situatii in care conexiunea a fost inchisa/pierduta in timpul transmiterii request-ului.
5. In cazul comenzilor `login_admin`, `login`, `get_access`, se salveaza cookie-ul de sesiune, respectiv token-ul JWT, acestea fiind transmise in forma de headere in request-urile ulterioare.
6. In cazul comenzilor `logout`, `delete_user`, se va sterge cookie-ul de sesiune, respectiv token-ul JWT, pentru a evita utilizarea acestora in request-urile ulterioare.

## Biblioteci externe

- `nlohmann/json`: biblioteca pentru parsarea JSON-ului. Am ales aceasta biblioteca datorita reputatiei sale, a usurintei in utilizare si a documentatiei excelente.
- `fmt`: biblioteca pentru formatarea string-urilor. Am folosit aceasta biblioteca pentru formatarea diferitelor string-uri din cod, in special pentru formatarea mesajelor de eroare si a path-urilor. Aceasta biblioteca a fost introdusa relativ recent in biblioteca standard, insa versiunea compilatorului folosita de catre checker nu o suporta.
- `spdlog`: biblioteca pentru logging. Am folosit aceasta biblioteca pentru a realiza logging-ul request-urilor si raspunsurilor.
- `ctre`: biblioteca pentru regex-uri compile time. Am folosit aceasta biblioteca in detrimentul `std::regex` pentru a evita overhead-ul care vine cu compilarea regex-urilor la runtime, avand totodata o sintaxa moderna.
- `scope_guard`: biblioteca pentru a realiza scope guard-uri. Am folosit aceasta biblioteca pentru a realiza anumite cleanup-uri intr-un mod elegant, evitand astfel codul duplicat sau `goto`-urile.
