# fldigi modernizācija

## Dokumenta statuss
Šis ir dzīvs modernizācijas plāns. Ja darba laikā rodas jauna ideja, arhitektūras lēmums vai nepieciešama virziena maiņa, to uzreiz pievienojam šim dokumentam. Obligātie darba noteikumi glabājas `PROJECT_RULES.md`.

## Projekta mērķis
Modernizēt fldigi, saglabājot tā modemu, DSP un radioamatieru funkcionalitāti, bet padarot konfigurāciju, profilus, PTT, audio, logbook, integrāciju un tulkošanu mūsdienīgāku un saprotamāku. Programmu nepārrakstām no nulles un neveidojam otru fldigi kopiju.

## Git un būves modelis
- `master` = SourceForge oficiālais fldigi 1:1.
- `modernizacija` = mūsu izmaiņas virs oficiālā koda.
- SourceForge izmaiņas vispirms nonāk `master`, pēc tam tiek integrētas `modernizacija`.
- `build/`, `deps/` un lokālie artefakti Git netiek glabāti.
- Pašlaik pārbaudīts: fldigi 4.2.13; FLTK 1.4.5 ar X11 ON un Wayland OFF; statisks PortAudio ar ALSA, PulseAudio un sndio; 2 PASS, 0 SKIP, 0 FAIL, 0 ERROR; Python 3.14.7 caur uv; DuckDB development C API.

## Profili
Profili tiek veidoti pēc YL3GBC modernizētā KLog principa. Paredzēts atsevišķs `Profila izvēle` logs ar `Izsaukuma signāls | QSO | Piezīme`, pogām `Atvērt`, `Atcelt`, `Jauns profils`, `Rediģēt`, `Dzēst` un automātiskas atvēršanas opciju. Katram profilam būs savi callsign, QTH, locator, radio/Hamlib, CAT, PTT, audio, logbook un integrāciju iestatījumi. Programmas vispārīgie GUI parametri paliek globāli.

## Konfigurācijas pārbūve
Configure tiek organizēts pēc lietotāja uzdevumiem: `Radio`, `PTT`, `Audio`, `Logbook`, `Integrācija`, `Modemi`, `Advanced`. Simple režīms ir noklusējums; zema līmeņa Hamlib, backend, device string, XML-RPC un citi tehniskie parametri tiek pārvietoti uz Advanced. Paredzēts vednis: callsign -> stacijas dati -> radio -> ports -> CAT -> PTT -> audio -> testi -> saglabāt profilu.

## Radio, PTT un audio
Radio sadaļā lietotājs izvēlas ražotāju un modeli, tehniskās Hamlib detaļas pēc iespējas nosakām automātiski. PTT pēc JTDX principa: savstarpēji izslēdzoši `VOX`, `CAT`, `DTR`, `RTS`, kā arī `Port`, `Share PTT port`, `Test PTT`, `Tx delay`. Audio pamata skatā tikai saprotami nosaukti Input/Output device; backend, sample rate, bufferi, latency un device string paliek Advanced.

## Logbook un DuckDB
Pašreizējais fldigi balstās uz ADIF failu un iekšējām C++ QSO struktūrām. Modernizācijā DuckDB kļūst par strukturētu logbook datu slāni, bet ADIF paliek importa, eksporta, backup un savietojamības formāts. QSO modelim jāatbalsta visi fldigi režīmi. Meklēšanai jāatbalsta callsign, band, frequency, mode, submode, datums, laiks, QTH, locator, profils, LoTW/ClubLog statusi un citi lauki. Precīzo DB shēmu definējam tikai pēc esošā logbook koda izpētes.

## KLog integrācija
fldigi jāsadarbojas ar YL3GBC modernizēto KLog. Programmas nekontrolēti nedala vienu DuckDB failu. Primāri izpētām `Log QSO -> QSO Logged -> UDP -> KLog`, līdzīgi WSJT-X/JTDX. Rezerves savietojamībai KLog var sekot fldigi `logbook.adif` un apstrādāt tikai jaunus ierakstus. Abiem ceļiem KLog pusē jāizmanto viena QSO ievietošanas un dublikātu pārbaudes loģika. Jāizpēta arī vecā KLog/fldigi integrācija un fldigi XML-RPC.

## Tulkojumi, migrācija un testēšana
Tulkotāju darba formāts paredzēts Qt Linguist `.ts`, saglabājot fldigi runtime gettext: `.ts -> .po/.mo`. Vecā konfigurācija nedrīkst pazust; paredzēta migrācija uz profilu modeli. Pēc katra lielā posma būvējam visu fldigi un izpildām visus testus; nedrīkst būt `SKIP`, `FAIL` vai `ERROR`. Saglabājam X11 bez obligāta Wayland.

## Darba secība
1. Uzturēt šo dzīvo plānu.
2. Izpētīt esošo Configure arhitektūru.
3. Ieviest profilu arhitektūru un profila izvēles logu.
4. Nodalīt profila un globālos iestatījumus.
5. Pārorganizēt Configure.
6. Ieviest JTDX tipa PTT.
7. Izpētīt veco KLog/fldigi integrāciju.
8. Izpētīt fldigi logbook datu plūsmu.
9. Definēt DuckDB QSO/profilu modeli.
10. Ieviest DuckDB un ADIF savietojamību.
11. Ieviest fldigi -> KLog integrāciju.
12. Ieviest `.ts` tulkošanas plūsmu.
13. Ieviest pirmās palaišanas vedni.
14. Pilna regresijas un migrācijas testēšana.
15. Sakārtot instalāciju un turpmāko SourceForge atjauninājumu integrāciju.

## Profilu arhitektūras lēmums
- `HomeDir` paliek globālā fldigi konfigurācijas sakne; profili nepārvieto visu `HomeDir`.
- Ieviešam atsevišķu profila konfigurācijas slāni (`ProfileDir`/ProfileSettings), kas tiek uzlikts virs globālajiem `progdefaults`.
- Startup secība: nolasīt globālo konfigurāciju -> izvēlēties profilu -> ielādēt profila laukus -> veidot galveno fldigi UI -> inicializēt radio, PTT un audio.
- Profilam pieder stacijas dati: callsign, QTH, vārds, locator un antena.
- Profilam pieder Radio/CAT, PTT, audio ierīces, logbook ceļš un ar konkrēto staciju saistītās integrācijas.
- GUI tēma, fonti, logu izmēri un citi programmas vispārīgie iestatījumi paliek globāli.
- Katram profilam būs savs logbook datu konteksts; DuckDB shēmu definēsim pēc esošā logbook koda izpētes.
- JTDX tipa PTT `VOX / CAT / DTR / RTS` būs normalizēts lietotāja slānis virs esošās fldigi PTT loģikas.

## CW / RTTY / PSK makrosu varianti
- Izmantojam jau esošo fldigi makrosu paneli un tā fiziskos makrosu taustiņus; jaunu makrosu paneli neveidojam.
- Izvēloties CW, RTTY vai PSK, parādās neliela papildizvēlne `Normal / Contest / DX`.
- Izvēlētais variants automātiski ielādē attiecīgo makrosu banku esošajos makrosu taustiņos.
- Pamata bankas: `CW-Normal`, `CW-Contest`, `CW-DX`, `RTTY-Normal`, `RTTY-Contest`, `RTTY-DX`, `PSK-Normal`, `PSK-Contest`, `PSK-DX`.
- Lietotāja interfeisā un KLog savietojamības līmenī izmantojam nosaukumu `PSK`, nevis `BPSK`.
- Konkrētais PSK paveids paliek fldigi iekšējā modema līmenī un neietekmē makrosu bankas nosaukumu.
