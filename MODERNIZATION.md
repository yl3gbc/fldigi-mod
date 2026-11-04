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
Configure tiek organizēts pēc lietotāja uzdevumiem: `Radio`, `PTT`, `Audio`, `Logbook`, `Integrācija`, `Modemi`, `Advanced`. Sadaļa `Logbook` saglabā nosaukumu, bet tas nav klasiskais fldigi logbook: tur ir tikai ADIF. Simple režīms ir noklusējums; zema līmeņa Hamlib, backend, device string, XML-RPC un citi tehniskie parametri tiek pārvietoti uz Advanced. Paredzēts vednis: callsign -> stacijas dati -> radio -> ports -> CAT -> PTT -> audio -> testi -> saglabāt profilu.

## Radio, PTT un audio
Radio sadaļā lietotājs izvēlas ražotāju un modeli, tehniskās Hamlib detaļas pēc iespējas nosakām automātiski. PTT pēc JTDX principa: savstarpēji izslēdzoši `VOX`, `CAT`, `DTR`, `RTS`, kā arī `Port`, `Share PTT port`, `Test PTT`, `Tx delay`. Audio pamata skatā tikai saprotami nosaukti Input/Output device; backend, sample rate, bufferi, latency un device string paliek Advanced.

## Logbook un QSO nodošana
fldigi nav un nebūs pilnas logbook. Katram profilam ir savs klasisks ADIF fails kontrolei: ADIF faila ceļš (`LOGBOOKFILENAME`) kļūst par profila lauku, un fldigi tajā ieraksta QSO. Logbook logs paliek tikai kā QSO uzskaite ar kolonnām: Call, datums, laiks UTC, diapazons metros (160m, 80m, 20m, 2m, 70cm...), mode, RST S, RST R, lokators (ja ir), QTH, vārds, ITU zona, CQ zona un distance (no lokatora, ja tas ir); meklēšanas funkcijas nav. ADIF failā QSO tiek ierakstīts pilnībā pēc ADIF standarta. Gan QSO ierakstu, gan operatora vārdu un QTH var ievadīt arī ar roku. Ārējo servisu nav; vienīgais izņēmums ir operatora vārda un QTH atrašana caur qrz.com un HamQTH. Vārda un QTH rādīšana uzskaitē ir sākotnējā izvēle; ja nederēs, to mainīsim. QSO tiek nodots ārējam žurnālam pa UDP kā ADIF ieraksts (esošais `post_udp_log()`). Visi Mac un Linux žurnāli atbalsta UDP; mērķis ir vienkārša un pareiza UDP pieslēgšana un konfigurācija. Dažiem žurnāliem UDP adrese un ports jāievada manuāli, tāpēc konfigurācijā tie paliek rediģējami. Jāuzraksta rokasgrāmatas, kā fldigi pieslēgt un nokonfigurēt UDP savienojumu ar katru populāro Mac un Linux žurnālu. Dublikātu pārbaude pagaidām notiek caur KLog. Sūtīšanu uz eQSL, LoTW, ClubLog, QRZ un citiem tiešsaistes žurnāliem veic KLog vai cits žurnāls; fldigi šīs funkcijas tiek izņemtas.

## Žurnālu integrācija
fldigi sadarbojas ar jebkuru Mac un Linux radioamatieru žurnālu (ap 20 populāru, regulāri parādās jauni) caur UDP. YL3GBC modernizētais KLog ir galvenais testa žurnāls. Jāizpēta arī vecā KLog/fldigi integrācija un fldigi XML-RPC.

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
8. Izpētīt fldigi logbook un UDP datu plūsmu.
9. Definēt DuckDB profilu modeli.
10. Vienkāršot fldigi logbook logu līdz QSO uzskaitei, izņemt meklēšanu un ārējos servisus (izņemot qrz.com un HamQTH vārda un QTH atrašanu); paturēt profila ADIF kontroles failu.
11. Sakārtot UDP QSO nodošanu žurnāliem.
12. Uzrakstīt UDP pieslēgšanas un konfigurēšanas rokasgrāmatas žurnāliem.
13. Ieviest `.ts` tulkošanas plūsmu.
14. Ieviest pirmās palaišanas vedni.
15. Pilna regresijas un migrācijas testēšana.
16. Sakārtot instalāciju un turpmāko SourceForge atjauninājumu integrāciju.

## Profilu arhitektūras lēmums
- `HomeDir` paliek globālā fldigi konfigurācijas sakne; profili nepārvieto visu `HomeDir`.
- Ieviešam atsevišķu profila konfigurācijas slāni (`ProfileDir`/ProfileSettings), kas tiek uzlikts virs globālajiem `progdefaults`.
- Startup secība: nolasīt globālo konfigurāciju -> izvēlēties profilu -> ielādēt profila laukus -> veidot galveno fldigi UI -> inicializēt radio, PTT un audio.
- Profilam pieder stacijas dati: callsign, QTH, vārds, locator un antena.
- Profilam pieder Radio/CAT, PTT, audio ierīces, logbook ceļš un ar konkrēto staciju saistītās integrācijas.
- GUI tēma, fonti, logu izmēri un citi programmas vispārīgie iestatījumi paliek globāli.
- Profili un to iestatījumi glabājas DuckDB. Profilam pieder arī UDP žurnāla pieslēgums (adrese, ports).
- Profilam pieder savs kontroles ADIF fails; profila izvēles loga kolonna `QSO` rāda QSO skaitu no šī faila.
- Profila lauku sadalījums (sākotnējais, vajadzības gadījumā mainām): stacija (6), kontesta dati (28), Radio/CAT ar flrig (48), PTT (17), audio ierīces un iestatījumi (19), CW/FSK atslēgošanas aparatūra (54), qrz.com/HamQTH pieejas dati (4), kontroles ADIF un UDP (4); kopā 180 `progdefaults` lauki. Princips: viss, kas saistīts ar staciju, radio vai tā interfeisu, pieder profilam.
- Globāli paliek brīdinājumu skaņa, GPIO (Raspberry Pi / Orange Pi), modemu iestatījumi, serveru URL, UI, krāsas, fonti un waterfall.
- WinKeyer sakaru iestatījumi glabājas `progStatus` (`fldigi.prefs`), nevis `progdefaults`, tāpēc profila slānim jāaptver arī tie.
- JTDX tipa PTT `VOX / CAT / DTR / RTS` būs normalizēts lietotāja slānis virs esošās fldigi PTT loģikas.

## Profilu glabāšana
- Katram profilam sava mape `~/.fldigi/profiles/<CALL>/`; mapes nosaukums ir pamata izsaukuma signāls lielajiem burtiem. Veidojot jaunu profilu, fldigi pamata izsaukuma signālu automātiski pārvērš lielajos burtos.
- Profils var būt tikai pamata izsaukuma signāls; izsaukuma signāli ar `/` nekad nav profili.
- Faili profila mapē ir ar mazajiem burtiem; `/` failu nosaukumos aizvieto ar `_`. `<call>.duckdb` ir profila DB, `<call>.adif` ir pamata signāla kontroles ADIF.
- Alternatīvie izsaukuma signāli (piem. `YL3GBC/P`, `YL3GBC/AM`, `SM/YL3GBC`) tiek reģistrēti profilā. Katram darbības periodam jānorāda datums no–līdz, lokators un pasākums; pasākuma lauks ir obligāts. Strādājot vēlreiz ar to pašu alternatīvo signālu, periods tiek norādīts no jauna.
- Katram periodam savs neatkarīgs ADIF fails, piem. `yl3gbc_p_2026-06-20.adif`; pasākuma nosaukums tiek ierakstīts ADIF galvenē.
- Profila DB tabulas: `profile` (callsign, note, created_at, updated_at), `settings` (tag, value; fldigi taga nosaukums un vērtība), `alt_callsigns` (callsign), `alt_activations` (callsign, date_from, date_to, locator, event).
- Automātiskā profila atvēršana glabājas globāli `fldigi_def.xml`.
- Profila izvēles logā pamata izsaukuma signāls ir augšējā rindā; zem tā parādās apakšsignāli, ar kuriem ir strādāts, katrs ar periodu, QSO skaitu un pasākumu. Kolonna `QSO` katrā rindā rāda tikai attiecīgā ADIF faila QSO skaitu.
- `Atvērt` uz apakšsignāla rindas atver profilu un uzreiz strādā ar šo apakšsignālu šajā periodā.
- Logā katram apakšsignālam redzams tikai pēdējais periods. Kad pasākums beidzies un ar to pašu apakšsignālu strādā no jauna, vecā perioda ADIF tiek pārvietots uz `~/.fldigi/profiles/<CALL>/archive/` un logā vairs nav redzams; fails netiek dzēsts.
- Profila logā ir poga `Arhīvs`, kas parāda arhivētos periodus tikai lasīšanai, lai nejauši nepārrakstītu nevienu QSO.
- Profila rediģēšanas logā ir cilnes `Stacija`, `Radio`, `Audio` un `Kontests`. `Stacija`: izsaukuma signāls (rakstot automātiski lielie burti), vārds, QTH, lokators, antena, piezīme un neobligāti alternatīvie izsaukuma signāli. `Radio` kā JTDX/WSJT-X: ražotājs, modelis, CAT ports, ātrums, `Test CAT`, PTT `VOX / CAT / DTR / RTS`, ports, `Tx delay`, tonis pirms raidīšanas (`pretone`), `Share PTT port`, `Test PTT`. `Audio`: Input un Output ierīce; tehniskie parametri paliek Advanced.
- Cilnē `Kontests` tiek savākti visi kontesta iestatījumi no visas programmas (pašreizējās Configure lapas `Contests/General`, `Contests/Info`, `Contests/Categories`, `Contests/Field Day`, `Contests/JOTA School`).
- Sacensībās QSO uzreiz aiziet uz žurnālu pa UDP. Kontesta režīma QSO tiek rakstīti neatkarīgā ADIF failā tā profila mapē ar sacensību nosaukumu un gadu, piem. `~/.fldigi/profiles/YL3GBC/cq_ww_rtty_2026.adif`; sacensību beigās no tiem tiek izveidots Cabrillo fails.

## CW / RTTY / PSK makrosu varianti
- Izmantojam jau esošo fldigi makrosu paneli un tā fiziskos makrosu taustiņus; jaunu makrosu paneli neveidojam.
- Izvēloties CW, RTTY vai PSK, parādās neliela papildizvēlne `Normal / Contest / DX`.
- Izvēlētais variants automātiski ielādē attiecīgo makrosu banku esošajos makrosu taustiņos.
- Pamata bankas: `CW-Normal`, `CW-Contest`, `CW-DX`, `RTTY-Normal`, `RTTY-Contest`, `RTTY-DX`, `PSK-Normal`, `PSK-Contest`, `PSK-DX`.
- Lietotāja interfeisā un KLog savietojamības līmenī izmantojam nosaukumu `PSK`, nevis `BPSK`.
- Konkrētais PSK paveids paliek fldigi iekšējā modema līmenī un neietekmē makrosu bankas nosaukumu.
