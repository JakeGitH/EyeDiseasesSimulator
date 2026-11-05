# Eye Diseases Simulator

[English](README.md) | **Polski**

Aplikacja Mixed Reality na **Meta Quest 3 / 3S**, która pokazuje, jak widzą osoby z wadami wzroku i chorobami oczu.
Użytkownik widzi swoje prawdziwe otoczenie z kamer gogli, a obraz jest **przetwarzany na żywo** (filtry, splot, macierze kolorów, maski), tak aby symulować wybraną chorobę.

Projekt zaliczeniowy z przedmiotu *Przetwarzanie obrazu*.

## Symulowane schorzenia

| Schorzenie | Co widać | Operacje przetwarzania obrazu |
|---|---|---|
| Protanopia / Deuteranopia / Tritanopia | daltonizm (brak rozróżniania czerwieni/zieleni lub niebieskiego/żółtego) | macierz 3×3 w liniowym RGB (Machado i in., 2009), interpolacja wg nasilenia |
| Achromatopsja | brak widzenia barw, słabsza ostrość, światłowstręt | konwersja do luminancji (Rec. 709), rozmycie, poświata |
| Zaćma | rozmycie, zamglenie, żółknięcie, olśnienie przy światłach | rozmycie Gaussa, mieszanie z kolorem mgły, spadek kontrastu, filtr bright-pass + rozmycie (glare) |
| Jaskra | zawężenie pola widzenia (widzenie tunelowe) | radialna maska z łagodnym przejściem, rozmycie peryferii |
| Zwyrodnienie plamki (AMD) | ciemna, rozmyta plama w centrum, falujące linie | maska centralna z zaszumioną krawędzią, przemieszczenie UV (metamorfopsja) |
| Krótkowzroczność / Dalekowzroczność / Prezbiopia | nieostre obiekty dalekie (krótkowzroczność) albo bliskie (dalekowzroczność, prezbiopia) | **rozmycie zależne od głębi**: mapa wergencji (1/odległość) z Depth API, model soczewki cienkiej, rozmycie Gaussa o zmiennej sigmie (interpolacja 3 poziomów) |
| Retinopatia cukrzycowa | ciemne plamy, rozmycie | progowany szum fraktalny (fBm), rozmycie, spadek kontrastu |

Większość schorzeń ma parametr **nasilenia** (0–1). Wady refrakcji ustawia się fizycznie: w **dioptriach** (np. −7 D, +4 D) i wiekiem (amplituda akomodacji).

### Rozmycie zależne od głębi (wady refrakcji)

1. **Mapa głębi**: siatka 40×30 promieni z kamery przez środki komórek obrazu, raycast po danych Depth API (MRUK `RaycastEnvironment`), 300 promieni na klatkę (round robin). Wynik zapisywany jako **wergencja** V = 1/d [D] w teksturze R16F, upsampling dwuliniowy w shaderze.
2. **Zakres ostrości oka**: oko z wadą R [D] bez akomodacji ogniskuje wergencję −R, akomodacja A dodaje do A dioptrii:
   `Vmin = −R`, `Vmax = −R + A`, gdzie `A = max(0, 18.5 − 0.3·wiek)` (wzór Hofstettera).
   Punkt daleki = 1/Vmin, punkt bliski = 1/Vmax.
3. **Rozogniskowanie** piksela: `ΔD = max(Vmin − V, V − Vmax, 0)`.
4. **Wielkość rozmycia**: krążek rozproszenia ma średnicę kątową `β = p·ΔD` (p = średnica źrenicy w m). Gaussian o tym samym momencie drugim co dysk o promieniu r ma `σ = r/2`, więc `σ_px = f_px · p · ΔD / 4` (f_px = ogniskowa kamery w pikselach).
5. **Rozmycie o zmiennej sigmie**: na GPU liczone są dwa poziomy (σ_max dla największego ΔD w kadrze i 0.35·σ_max), a każdy piksel interpoluje liniowo między obrazem ostrym, poziomem środkowym i maksymalnym.

Przykład (wiek 45 → A = 5 D): −3 D: ostro od 13 cm do 33 cm. +3 D: ostro od 50 cm do nieskończoności. +6 D: nic nie jest ostre.

## Jak to działa

```
Kamera passthrough Questa (Meta XR: Passthrough Camera Access)
        │  klatka 1280×960, sRGB
        ▼
VisionShaders (HLSL, Render Dependency Graph)
   DownsamplePS   → zmniejszenie 2^n×  (+ bright-pass dla poświaty)
   GaussianBlurPS → separowalny splot Gaussa (poziomo, potem pionowo)
   CompositePS    → maski rozmycia, rozmycie wg głębi, poświata, macierz
                    kolorów, mgła, kontrast, winieta, skotoma, zniekształcenie,
                    plamy, tryb porównania
        ▲
        │ mapa wergencji 40×30 (R16F)
UVisionDepthMap ← raycasty po Depth API (MRUK)
        │
        ▼
Render target → quad przed oczami, ustawiony wg parametrów kamery
(ogniskowa, punkt główny, przesunięcie obiektywu względem gogli)
```

Najważniejsze pliki:

| Plik | Rola |
|---|---|
| `Shaders/Private/VisionEffects.usf` | wszystkie algorytmy przetwarzania obrazu (HLSL) |
| `Source/VisionShaders/` | moduł shaderów: klasy shaderów i kolejność przebiegów (RDG) |
| `Source/EyeDiseasesSimulator/VisionConditionPresets.cpp` | parametry każdego schorzenia, model optyczny wad refrakcji (`VisionOptics`) |
| `Source/EyeDiseasesSimulator/VisionDisplayActor.*` | kamera, uprawnienia, przetwarzanie co klatkę, wyświetlanie, rozmycie na dioptrię |
| `Source/EyeDiseasesSimulator/VisionDepthMap.*` | mapa głębi (wergencji) z raycastów Depth API |
| `Source/EyeDiseasesSimulator/VisionSimulatorSubsystem.*` | stan symulatora (`FVisionSettings`), API dla menu |
| `Source/EyeDiseasesSimulator/HandMenuActor.*` | menu przy dłoni (ogólne, do przeniesienia do szablonu) |
| `Source/EyeDiseasesSimulator/VisionMenuWidget.*` | logika menu (strony, Back, suwak, Exit z potwierdzeniem) |
| `Source/EyeDiseasesSimulator/VisionHandMenuWidget.*` | wygląd menu zbudowany w C++ |
| `Source/EyeDiseasesSimulator/VisionConditionLibrary.*` | nazwy i opisy schorzeń dla UI |
| `Tools/CreateVisionAssets.py` | tworzy materiał `M_VisionFeed` w edytorze |

## Wymagania

- Unreal Engine **5.6**
- Plugin **Meta XR** (testowane z wersją 1.117), który zawiera Passthrough Camera Access
- Visual Studio 2022 z obsługą C++, Android SDK/NDK skonfigurowane dla UE
- Meta Quest 3 lub 3S w trybie deweloperskim (Horizon OS v74+)

## Pierwsze uruchomienie

1. Sklonuj repozytorium (z **Git LFS**: `git lfs install` przed klonowaniem).
2. Kliknij prawym na `EyeDiseasesSimulator.uproject` → *Generate Visual Studio project files*, potem otwórz projekt (skompiluje moduły C++).
3. Utwórz materiał wyświetlający obraz: w edytorze *Tools → Execute Python Script* → `Tools/CreateVisionAssets.py`.
4. Spakuj projekt na Androida (*Platforms → Android → Package Project*) i zainstaluj APK na goglach.
5. Przy pierwszym uruchomieniu zezwól aplikacji na dostęp do kamery gogli i do danych przestrzennych (potrzebne do głębi).

Boundary (Guardian) jest wyłączony: `bBoundaryVisibilitySupportEnabled` + `bDefaultBoundaryVisibilitySuppressed` w `Config/DefaultEngine.ini` (Meta XR: Boundary Visibility API, działa tylko przy aktywnym passthrough). Można chodzić poza wyznaczony obszar bez przerywania aplikacji.

> **Uwaga:** bez boundary nic nie ostrzega przed przeszkodami. Przy silnych efektach (zaćma, jaskra, AMD) obraz jest celowo ograniczony, więc chodzić należy ostrożnie.

## Sterowanie

Aplikacja startuje z normalnym obrazem. **Menu przy lewej dłoni** pojawia się, gdy obrócisz wnętrze dłoni do twarzy i na nią spojrzysz.
Przyciski wciska się palcem wskazującym prawej dłoni (poke) albo promieniem (ray).

- **Lista schorzeń**: wybranie schorzenia włącza je i zmienia stronę menu, więc przypadkowe dotknięcie nie przełączy na inne.
- **Back**: wyłącza schorzenie i wraca do listy.
- **Krótkowzroczność / dalekowzroczność**: suwak w dioptriach (−10…−0.25 D / +0.25…+6 D) oraz informacja, w jakim zakresie odległości obraz jest ostry.
- **Prezbiopia**: suwak wieku (40–65 lat; powyżej ~62 lat akomodacja wg Hofstettera wynosi 0).
- **Compare** (stopka menu): lewa połowa obrazu bez zmian, prawa przetworzona.
- **Exit** (stopka menu): pyta o potwierdzenie (Cancel / Exit), dopiero potem zamyka aplikację.

Menu (układ i styl) jest zbudowane w C++: `UVisionHandMenuWidget`. Opcjonalnie `Content/VisionSim/UI/BP_HandMenu` (rodzic: `HandMenuActor`) do ustawienia przesunięć w edytorze lub podmiany widgetu.

### Zmienne konsoli

| Zmienna | Domyślnie | Opis |
|---|---|---|
| `vision.AutoCycleSeconds` | 0 | automatyczne przełączanie schorzeń co N s (demo bez menu) |
| `vision.PupilDiameterMm` | 5 | średnica źrenicy, skaluje rozmycie wad refrakcji |
| `vision.DefocusScale` | 2 | mnożnik rozmycia (1 = fizycznie; 2 kompensuje rozdzielczość kamery i wyświetlacza) |
| `vision.DebugView` | 0 | 1 = mapa głębi, 2 = sigma rozmycia (do dokumentacji) |
| `vision.Depth.RaysPerFrame` | 300 | liczba raycastów głębi na klatkę |
| `vision.Depth.FallbackDistance` | 2 | odległość [m] przyjmowana bez danych o głębi |
| `handmenu.Debug` | 0 | rysuje normalną dłoni i stan wykrywania |

Można je ustawić w `Config/DefaultEngine.ini` w sekcji `[ConsoleVariables]`.

## Status

- [x] Dostęp do klatek z kamer (Passthrough Camera Access)
- [x] Potok przetwarzania obrazu w shaderach (rozmycie, poświata, macierze kolorów, maski, szum)
- [x] Parametry wszystkich schorzeń
- [x] Test na goglach: obraz z kamer i efekty działają
- [ ] Pomiar wydajności (cel: 72 FPS)
- [ ] Obraz stereo (osobna kamera dla każdego oka)
- [x] Logika menu przy dłoni (wykrywanie dłoni, strony menu, suwaki)
- [x] Wygląd menu (C++, `UVisionHandMenuWidget`)
- [x] Rozmycie zależne od głębi (krótko-, dalekowzroczność, prezbiopia)
- [x] Wyłączony boundary, przycisk Exit z potwierdzeniem
- [x] Tryb porównania: obraz zdrowy / chory

## Źródła

- G. M. Machado, M. M. Oliveira, L. A. F. Fernandes, *A Physiologically-based Model for Simulation of Color Vision Deficiency*, IEEE TVCG 15(6), 2009.
- H. W. Hofstetter, *Useful age-amplitude formula*, Optometry World 38, 1950 (amplituda akomodacji w funkcji wieku).
- Meta, *Depth API* / MR Utility Kit *Environment Raycasting*: dokumentacja Meta Horizon OS dla deweloperów.
- Meta, *Passthrough Camera API* — dokumentacja Meta Horizon OS dla deweloperów.
