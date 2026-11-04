# Eye Diseases Simulator

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
| Krótkowzroczność / Dalekowzroczność | nieostry obraz | rozmycie Gaussa (na razie jednolite, bez głębi) |
| Retinopatia cukrzycowa | ciemne plamy, rozmycie | progowany szum fraktalny (fBm), rozmycie, spadek kontrastu |

Każde schorzenie ma parametr **nasilenia** (0–1).

## Jak to działa

```
Kamera passthrough Questa (Meta XR: Passthrough Camera Access)
        │  klatka 1280×960, sRGB
        ▼
VisionShaders (HLSL, Render Dependency Graph)
   DownsamplePS   → zmniejszenie 2^n×  (+ bright-pass dla poświaty)
   GaussianBlurPS → separowalny splot Gaussa (poziomo, potem pionowo)
   CompositePS    → maski rozmycia, poświata, macierz kolorów, mgła,
                    kontrast, winieta, skotoma, zniekształcenie, plamy
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
| `Source/EyeDiseasesSimulator/VisionConditionPresets.cpp` | parametry każdego schorzenia w zależności od nasilenia |
| `Source/EyeDiseasesSimulator/VisionDisplayActor.*` | kamera, uprawnienia, przetwarzanie co klatkę, wyświetlanie |
| `Source/EyeDiseasesSimulator/VisionSimulatorSubsystem.*` | aktualne schorzenie i nasilenie (API dla menu/UI) |
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
5. Przy pierwszym uruchomieniu zezwól aplikacji na dostęp do kamery gogli.

## Sterowanie (wersja tymczasowa)

Dopóki nie ma menu przy dłoni, schorzenia przełączają się automatycznie co 6 sekund.
Steruje tym zmienna konsoli `vision.AutoCycleSeconds` (0 = wyłączone). Można ją ustawić w `Config/DefaultEngine.ini`:

```ini
[ConsoleVariables]
vision.AutoCycleSeconds=10
```

## Status

- [x] Dostęp do klatek z kamer (Passthrough Camera Access)
- [x] Potok przetwarzania obrazu w shaderach (rozmycie, poświata, macierze kolorów, maski, szum)
- [x] Parametry wszystkich schorzeń
- [ ] Test na goglach: wyrównanie obrazu z rzeczywistym widokiem, wydajność (cel: 72 FPS)
- [ ] Obraz stereo (osobna kamera dla każdego oka)
- [ ] Menu przy dłoni: wybór schorzenia, suwak nasilenia
- [ ] Rozmycie zależne od głębi (krótko- i dalekowzroczność)
- [ ] Tryb porównania: obraz zdrowy / chory

## Źródła

- G. M. Machado, M. M. Oliveira, L. A. F. Fernandes, *A Physiologically-based Model for Simulation of Color Vision Deficiency*, IEEE TVCG 15(6), 2009.
- Meta, *Passthrough Camera API* — dokumentacja Meta Horizon OS dla deweloperów.
