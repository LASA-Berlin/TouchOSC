# TouchOSC
Eine Library zur Kommunikation zwischen dem ESP8266 und der TouchOSC-App. Dieses Repository ist abgeleitet von dem Make TouchOSC Projekt

Damit die Parameter und Werte richtig ausgelesen werden muss die Bedienelement in TouchOSC folgende Struktur haben:
/[Kontrol-Typ]/[eindeutige ID]/[diese Werte werden von TouchOSC automatisch zugewisenen]
Beispiele:
/fader/1234
/rotary/50
/label/999
/KnopfFuerReset/404

Zum auslesen ist nur Wichtig das ein Name zugweisen ist und muss nicht den spezifische bezeichnung sein. 
Zum senden an TouchOSC muss jedoch der genaue Bezeichnung (zu finden auch im Header) zugewiesen sein. Die Zahlen sind frei wählbar und dienen zur eindeutigen Identifikation. 
Bei Mulltibuttons und Multifadern weist TouchOSC automatisch zusätzliche Indexe zu.
So wird beim bedienen des zweiten Faders aus bei /mutlifader/1000 ein /multifader/1000/2
Bei Einer Matrix aus Buttons passiet dies mit einem zusätzlichen index.

Da die App Befehle nur einmal sendet wenn ein Bedienelement verstellt wird sollte `read()` regelmäßig aufgerufen werden.

keywords.txt wird von der Arduino IDE genutzt, da diese ansonsten nicht Klassen und Methoden nicht farblich makieren kann.


## Referenz zu den Funktionen in der Klasse TouchOSC

### Konstruktor für TouchOSC Klasse
```c++
ToushOSC obj(int port)
```
obj: name des objektes  
port: Port für UDP (in der App standartmäßig 8000)

### öffentliche Methoden:
```c++
void read(String *controlTyp, int *parameternummer, int *idx1, int *idx2, int *val_1, int *val_2);
```
Liest UDP-Packet welche vom TouchOSC gesendet werden und liefert Parameter auf entsprechende Adressen
Nimmt Adressen, bzw Pointer von bis zu 6 verschiedenen Varriabeln und liefert entsprechende Parameter zurück  
Für nicht verwendete Parameter einfach die Varriable als Argument initialieseren  
read() ruft noch runtime() auf für Timeouts und andere Hintergrundsprosesse  

controlTyp: Names des Element  
parameternummer: eindeutige ID  
idx1: Index 1 für Multibuttons und Fader  
idx2: Index 2 für Multibuttons  
val_1: Ausgelesende Wert des entsprechendes Element  
val_2: Zweiter wert das XY-Pad  

```c++
void send(int type, int param, int idx_1, int idx_2, int val_1, int val_2);
```
Sendet Werte an spezifisiertes Bedienfled  

type: Typ-Nummer nach ControlNames[], kann via der enum im Header ausgewählt werden  
`0 = t_none, 1 = t_page, 2 = t_label, 3 = t_led, 4 = t_toggle, 5 = t_push, 6 = t_fader, 7 = t_rotary, 8 = t_multitoggle, 9 = t_multipush, 10 = t_multifader, 11 = t_xypad`  
param: Eindeutige ID des Bedienfeldes  
idx_1: Index, falls es sich um einen Button oder Fader handelt, sonst auf -1 setzten  
idx_2: Index, falls eine Matrix aus Buttons angesprochen wird, sonst auf -1 setzten  
val_1: Wert auf den das Element gesetzt werden soll  
val_2: Zweiter Wert falls benötigt, sonst auf -1 setzten  

```c++
void sendText(int type, int param, String textstr);
```
Gedacht um Texte an Labels zu schicken  

type: Typ des Bedienfeldes, hier eigentlich immer Label  
param: Eindeutige ID des Bedienfeldes  
textstr: Der zu sende Text

```c++
void readNonStop();
```
Gibt empfangene Packete auf dem Serielen Monitor wieder 
