# Tema 2 - Aplicatie client-server TCP/UDP

## Implementare

### Server

Serverul este implementat sub forma clasei **Server** din `src/server`. La constructie se creaza 2 socket-uri, unul pentru mesajele UDP (`udp_fd_`) si unul pentru stabilirea conexiunilor cu subscriberii TCP (`listen_fd_`), si de asemenea
se adauga in vectorul de structuri `pollfd` folosit pentru multiplexare. Daca se intampina o eroare la constructie, o exceptie este aruncata.

Pornirea serverului se face prin apelul metodei `run()`, care intra intr-un event loop ce apeleaza `poll()` pentru a astepta evenimente pe socket-urile adaugate in vectorul de structuri `pollfd`.

Pentru inputul de la `stdin`, singura comanda valida este **exit**, caz in care rularea se va opri. Orice alta comanda este ignorata.

Pentru conexiunile TCP noi acceptate pe socket-ul `listen_fd_`, serverul va incerca sa dezactiveze algoritmul lui Nagle (`TCP_NODELAY`) pe socket-ul corespunzator conexiunii, iar in caz de esec conexiunea va fi inchisa. In caz contrar,
socket-ul va fi adaugat in vectorul de structuri `pollfd` pentru a putea fi monitorizat.

Cand un mesaj soseste pe un socket TCP al unui subscriber, serverul deserializeaza mesajul si executa comanda corespunzatoare (`CONNECT`, `SUBSCRIBE`, `UNSUBSCRIBE`). In cazul in care mesajul nu este un request sau request-ul este invalid, o exceptie este aruncata (prinsa in loop-ul principal) si conexiunea este inchisa. De asemenea, conexiunea mai este inchisa si in cazul in care un subscriber incearca sa se conecteze cu un id deja conectat la momentul respectiv sau daca incearca sa execute o comanda inainte de a se fi trimis un mesaj de tip `CONNECT`. Informatiile subscriberilor cat si statusul lor (conectat/deconectat) sunt stocate in clasa **SubscribersRegistry**, care retine asocieri intre id-uri, socket-uri si informatiile subscriberilor, precum si o asociere intre topicurile existente si subscriberii abonati la ele. Astfel este foarte usor de verificat daca un subscriber exista in registru sau daca este conectat, dar si de a intoarce un set de subscriberi conectati care sunt abonati la un anume topic.

La primirea unui mesaj UDP, serverul incearca sa il deserializeze (similara cu deserializarea din protocolul TCP). In caz de esec mesajul este ignorat. In caz contrar, serverul verifica daca topicul are un format valid si foloseste setul de socket-uri ale subscriberilor abonati la topicuri care dau match cu topicul mesajului UDP (intors de metoda `retrieve_topic_subscribers(topic)` din **SubscribersRegistry**). Apoi, serverul trimite mesajul de raspuns catre toti subscriberii din set.

Rularea se realizeaza pana la oprire prin comanda **exit** sau pana la intampinarea unei erori critice, caz in care nu se mai poate face error handling.

### Client

Clientul este implementat sub forma clasei **Client** din `src/client`. Constructia si rularea clientului se realizeaza intr-o maniera similara cu cea a serverului.

La inceperea rularii, clientul se va conecta la serverul TCP si va trimite un mesaj de tip `CONNECT` cu id-ul sau, urmand ca apoi sa intre in event loop.

Comenzile valide pentru client sunt `exit`, `subscribe <topic>` si `unsubscribe <topic>`, orice alta comana fiind ignorata. La introducerea unei comenzi valide, topicul este verificat. In cazul in care acesta este valid, un request TCP corespunzator este trimis catre server. La fel ca in cazul implementarii serverului, la primirea unui mesaj TCP din partea serverului, acesta este deserializat si in cazul in care este valid, se afiseaza in consola un mesaj cu formatul din enunt `<IP_CLIENT_UDP>:<PORT_CLIENT_UDP> - <TOPIC> - <TIP_DATE> - <VALOARE_MESAJ>`.

Rularea se realizeaza pana la oprirea prin comanda **exit**, pana la intampinarea unei erori critice sau pana cand serverul TCP inchide conexiunea.

### Topicuri

Intrucat topicurile reprezinta niste pattern-uri, am ales ca le implementez sub forma clasei `TokenPattern`.

Am considerat ca fiecare topic este format din mai multe token-uri separate prin `/`, iar fiecare token poate fi fie un wildcard (`*`, `+`), fie un string. De asemenea, am considerat ca un pattern este invalid daca acesta contine doua wildcard-uri alaturate.

Matching-ul se face prin metoda `TokenPattern::matches(&other)`, care incearca sa dea match pattern-ului curent cu pattern-ul `other`. Algoritmul functioneaza pe principiul unui BFS, atunci cand se intalneste un wildcard `*` incercandu-se un matching de tip greedy. De asemenea, pattern-ul `other` nu are voie sa contina wildcard-uri.

### Eficienta

Eficienta implementarii vine atat din protocolul TCP folosit pentru comunicarea cu subscriberii (descris mai jos) care permite interpretarea rapida a mesajelor si folosirea unui buffer alocat o singura data, dar si din celelalte structuri de date folosite. De exemplu, `SubscribersRegistry` retine o asociere `topic_pattern -> subscribers` intr-un `std::unordered_map` care ajuta mult atunci cand mai multi subscriberi sunt abonati la acelasi topic, matching-ul facandu-se o singura data.

### Ierarhie

```
src
├── common
│   ├── proto_utils.hpp
│   ├── tcp_proto.cpp
│   ├── tcp_proto.hpp
│   ├── tcp_utils.cpp
│   ├── tcp_utils.hpp
│   ├── token_pattern.cpp
│   ├── token_pattern.hpp
│   └── util.hpp
├── server
│   ├── main.cpp
│   ├── server.cpp
│   ├── server.hpp
│   ├── subscribers_registry.cpp
│   ├── subscribers_registry.hpp
│   ├── udp_proto.cpp
│   └── udp_proto.hpp
└── tcp-client
    ├── client.cpp
    ├── client.hpp
    └── main.cpp
```

## Protocolul TCP

Protocolul proiectat este gandit sa fie transparent pentru utilizator, la nivel inalt lucrandu-se doar cu o ierarhie prestabilita de structuri pe baza principiului de encapsulare.

Ierarhia de structuri este urmatoarea:

```
#
└── TcpMessage
├── TcpRequest
│ ├── TcpRequestPayloadId
│ └── TcpRequestPayloadTopic
└── TcpResponse
  ├── TcpResponsePayloadInt
  ├── TcpResponsePayloadShortInt
  ├── TcpResponsePayloadFloat
  └── TcpResponsePayloadString
```

In varful ierarhiei se gaseste **TcpMessage** care encapsuleaza fie un **TcpRequest** fie un **TcpResponse**.

**TcpRequest** reprezinta mesajul transmis de catre client/subscriber catre server, iar **TcpResponse** reprezinta mesajul transmis de catre server catre client/subscriber in momentul in care un mesaj este receptionat prin protocolul UDP.

**TcpRequest** encapsuleaza un **TcpRequestPayloadId** sau un **TcpRequestPayloadTopic**, dar si tipul request-ului `CONNECT`, `SUBSCRIBE`, `UNSUBSCRIBE`.

**TcpResponse** contine ip-ul si portul clientului udp care a trimis mesajul, topicul pe care s-a trimis mesajul si un payload care poate fi unul dintre: **TcpResponsePayloadInt**, **TcpResponsePayloadShortInt**, **TcpResponsePayloadFloat**, **TcpResponsePayloadString**.
Aceste payload-uri sunt aceleasi din punct de vedere al structurii cu cele din protocolul de UDP, insa am tinut sa le decuplez pentru a nu aparea probleme daca unul dintre ele este eventual modificat in viitor.

Encapsularea mai multor structuri intr-un singur camp **payload** se face, in toate cazurile de mai sus, prin folosirea unui union (`std::variant` in c++17) care simplifica cu mult munca, facilitand, intr-o maniera sigura, stocarea tuturor tipurilor necesare.

Transparenta protocolului vine din folosirea serializarii si deserializarii mesajelor. Un mesaj serializat are forma:

```
---------------------------------------------------------------------------------------------
| TcpMessageType (1 byte) | TcpMessagePayloadLength (2 bytes) | TcpMessagePayload (N bytes) |
---------------------------------------------------------------------------------------------
```

unde:

- **TcpMessageType** este un byte care contine tipul mesajului (request sau response). Pe baza acestui tip se va modifica tipul campului `payload` din structura `TcpMessage` pentru a putea deserializa mesajul.
- **TcpMessagePayloadLength** este un `uint16_t` care contine lungimea payload-ului. Acesta este necesar pentru a putea deserializa mesajul, avand in vedere ca payload-ul poate fi de dimensiuni variate.
- **TcpMessagePayload** reprezinta restul datelor din mesaj care vor fi deserializate.

Serializarea este facuta prin simplul apel al functiei `TcpMessage::serialize(&msg, *buffer)` care serializeaza mesajul `msg` in buffer-ul `buffer`. Serializarea se face intr-un mod recursiv, fiecare structura din ierarhie stiind sa isi serializeze propriile campuri,
iar pentru structurile care contin un `std::variant` se va apela functia de serializare a tipului curent din `std::variant`.

Pentru deserializare, utilizatorul doar trebuie sa citeasca primii 3 bytes din mesajul primit: tipul mesajului si lungimea payload-ului (care ii indica cat sa citeasca in continuare). Dupa citirea payload-ului si setarea tipului mesajului primit,
deserializarea se face prin apelul functiei `deserialize(&msg, *buffer, buffer_size)` corespunzator tipului mesajului (din `TcpResponse` sau `TcpRequest`). Deserializarea este facuta in aceeasi maniera ca si serializarea. Astfel utilizatorul nu trebuie sa aiba
decat o minima cunostinta despre structura transmisa, restul fiind abstractizat.

Aceasta abordare de a serializa si deserializa mesajele are mai multe avantaje peste transmiterea unor mesaje de lungime fixa sau folosirea unor structuri **packed**:

- **Flexibilitate**: mesajele pot avea dimensiuni variate, in functie de payload-ul transmis, fara a fi nevoie de modificari in codul clientului sau serverului.
- **Portabilitate**: mesajele sunt serializate intr-un format standardizat, care poate fi usor interpretat pe orice platforma. Acest lucru este cu atat mai vizibil atunci cand comparam cu implementarile ce folosesc structuri **packed**.
- **Abstractizare**: utilizatorul nu are nevoie sa cunoasca detaliile implementarii protocolului sau layout-ul structurilor serializate. Acesta va lucra doar cu structurile puse la dispozitie.

Cateva detalii de implementare a protocolului:

- orice string care intra in continutul unui mesaj va fi precedat de lungimea sa (excluzand terminatorul `\0`), iar string-ul este transmis fara terminatorul `\0`.
- fiecare structura/payload are o lungime de serializare maxima exprimata prin constanta `MAX_SERIALIZED_SIZE`. Aceasta este folosita pentru a putea folosi buffere de lungime fixa pentru transmiterea si receptionarea mesajelor. De asemenea,
  lungimea serializata a mesajului curent se poate calcula prin apelul functiti `serialized_size()`.
