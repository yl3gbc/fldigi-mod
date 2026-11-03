# fldigi projekta darba noteikumi

## Obligātie noteikumi
- Viens fldigi pirmkoda koks un viens repozitorijs; otra kopija netiek veidota.
- `master` vienmēr ir SourceForge oficiālais fldigi 1:1; mūsu darbs notiek `modernizacija` zarā.
- Oficiālās izmaiņas vispirms ienāk `master`, pēc tam tiek integrētas `modernizacija`.
- Visa programmatūra un atkarības tiek būvētas no pirmkoda; `apt`, `snap`, `flatpak` un gatavi binārie bootstrap risinājumi ir aizliegti.
- Izmantojam development/main/master zarus; stabilas release/tag versijas bez skaidras atļaujas neizmantojam.
- fldigi projektā Python ir 3.14.7 un tiek palaists tikai ar `uv`.
- X11 ir obligāts; Wayland nedrīkst kļūt par obligātu atkarību.
- fldigi darbā izmantojam tikai `~/fldigi`; lokālās `Dokumenti` un `GitHub` direktorijas neaiztiekam.
- Komandās neizmantojam absolūtos ceļus un `Path.home()` bez skaidras atļaujas.
- `build/`, `deps/` un citi lokālie būves artefakti Git netiek commitoti.
- Nekādas destruktīvas darbības bez skaidras lietotāja atļaujas.
- Testiem jābeidzas bez `SKIP`, `FAIL` un `ERROR`; neko mākslīgi neizlaižam.
- Neizmantot standarta Linux pieņēmumus; vispirms pārbaudīt konkrēto projekta vidi.
- Lietotājs nosaka projekta struktūru; arhitektūru vai jau izvēlēto pieeju patvaļīgi nemainām.
- Visām neizmantotajām `.env` rindām jābūt komentētām ar `#`; tukšas aktīvas vērtības nav pieļaujamas.

## Darba komunikācijas noteikumi
- Komandas un kodu dodam tikai Python blokos ar `uv run python - <<'PY'` ... `PY` apvalku.
- Vienā Python blokā ne vairāk par 23 rindām, ieskaitot apvalku un tukšās rindas.
- Ja jau atrodamies `~/fldigi`, lieku `cd` nedodam.
- Pirms katra koda bloka norādām aktīvās prasības un precīzu rindu skaitu.
- Pirms būtiskas izmaiņas vispirms pārbaudām `PROJECT_RULES.md` un `MODERNIZATION.md`.

## Dokumentācijas princips
- Ja parādās jauns obligāts darba noteikums, papildinām šo failu.
- Ja parādās jauna fldigi funkcijas vai arhitektūras ideja, papildinām `MODERNIZATION.md`.
- Abi dokumenti ir dzīvi un tiek uzturēti kopā ar projekta kodu.
