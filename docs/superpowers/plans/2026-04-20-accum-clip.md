# Accumulator Clip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Añadir un parámetro `ACCUM_CLIP_PARAM` al módulo UZZ que reinicia globalmente todos los offsets de acumulación cada N sumas acumuladas.

**Architecture:** Un único nuevo parámetro snap (0=OFF, 1-16) y un contador de runtime `accumGlobalCount`. Toda la lógica está en `src/UZZ.cpp`. El nuevo widget `AccumDisplay` reemplaza al `ParamDisplay` de ACCUM_AMT mostrando dos líneas (semitones / clip). El layout cambia de 1 knob grande a 2 `Trimpot` pequeños.

**Tech Stack:** C++11, VCV Rack SDK (Rack API: `configParam`, `TransparentWidget`, `Trimpot`)

---

## Archivos a modificar

- **Modify:** `src/UZZ.cpp` — todos los cambios están aquí

---

### Task 1: Añadir `ACCUM_CLIP_PARAM` al enum y configurarlo

**Files:**
- Modify: `src/UZZ.cpp:529-536` (enum ParamIds) y `src/UZZ.cpp:663-664` (constructor)

- [ ] **Step 1: Añadir el param al enum**

En `src/UZZ.cpp`, localizar el bloque (alrededor de línea 530-536):
```cpp
    // New params (appended to preserve patch compatibility)
    ENUMS(PROB_PARAMS, 16),
    PROB_GLOBAL_PARAM,
    RND_PROB_PARAM,
    PROB_SHIFT_DOWN_PARAM,
    PROB_SHIFT_UP_PARAM,

    NUM_PARAMS
```
Reemplazar por:
```cpp
    // New params (appended to preserve patch compatibility)
    ENUMS(PROB_PARAMS, 16),
    PROB_GLOBAL_PARAM,
    RND_PROB_PARAM,
    PROB_SHIFT_DOWN_PARAM,
    PROB_SHIFT_UP_PARAM,
    ACCUM_CLIP_PARAM,

    NUM_PARAMS
```

- [ ] **Step 2: Configurar el param en el constructor**

Localizar (alrededor de línea 663-664):
```cpp
    configParam(ACCUM_AMT_PARAM, 0.f, 24.f, 1.f, "Accumulator amount", " st");
    paramQuantities[ACCUM_AMT_PARAM]->snapEnabled = true;
```
Reemplazar por:
```cpp
    configParam(ACCUM_AMT_PARAM, 0.f, 24.f, 1.f, "Accumulator amount", " st");
    paramQuantities[ACCUM_AMT_PARAM]->snapEnabled = true;
    configParam(ACCUM_CLIP_PARAM, 0.f, 16.f, 0.f, "Accumulator clip (reset every N sums, 0=OFF)");
    paramQuantities[ACCUM_CLIP_PARAM]->snapEnabled = true;
```

- [ ] **Step 3: Compilar para verificar que no hay errores**

```bash
RACK_DIR=../Rack-SDK make -j$(nproc) 2>&1 | grep -E "error:"
```
Resultado esperado: sin líneas de error.

- [ ] **Step 4: Commit**

```bash
git add src/UZZ.cpp
git commit -m "feat: add ACCUM_CLIP_PARAM enum + configParam"
```

---

### Task 2: Añadir estado de runtime y reset

**Files:**
- Modify: `src/UZZ.cpp:602` (estado), `src/UZZ.cpp:728-729` (onReset), `src/UZZ.cpp:1076-1077` (reset input)

- [ ] **Step 1: Añadir `accumGlobalCount` al struct**

Localizar (alrededor de línea 602):
```cpp
  int accumOffset[16] = {};
```
Reemplazar por:
```cpp
  int accumOffset[16] = {};
  int accumGlobalCount = 0;
```

- [ ] **Step 2: Limpiar el contador en `onReset()`**

Localizar (alrededor de línea 728-729):
```cpp
    for (int i = 0; i < 16; ++i)
      accumOffset[i] = 0;
  }
```
Reemplazar por:
```cpp
    for (int i = 0; i < 16; ++i)
      accumOffset[i] = 0;
    accumGlobalCount = 0;
  }
```

- [ ] **Step 3: Limpiar el contador cuando llega señal de RESET input**

Localizar (alrededor de línea 1076-1077 en `process()`):
```cpp
      for (int i = 0; i < 16; ++i)
        accumOffset[i] = 0;
```
Reemplazar por:
```cpp
      for (int i = 0; i < 16; ++i)
        accumOffset[i] = 0;
      accumGlobalCount = 0;
```

- [ ] **Step 4: Compilar**

```bash
RACK_DIR=../Rack-SDK make -j$(nproc) 2>&1 | grep -E "error:"
```

- [ ] **Step 5: Commit**

```bash
git add src/UZZ.cpp
git commit -m "feat: add accumGlobalCount state, reset on onReset + RESET input"
```

---

### Task 3: Lógica de acumulación con clip

**Files:**
- Modify: `src/UZZ.cpp:1151-1158` (bloque de acumulación en process())

- [ ] **Step 1: Reemplazar el bloque de acumulación**

Localizar (alrededor de línea 1151-1158):
```cpp
      if (prevMode == SM_ACCUM_UP || prevMode == SM_ACCUM_DOWN) {
        int amt = (int)std::round(params[ACCUM_AMT_PARAM].getValue());
        int signedAmt = (prevMode == SM_ACCUM_UP) ? amt : -amt;
        int v = accumOffset[prevStep] + signedAmt;
        const int range = 25; // -12..+12 inclusive
        v = ((v + 12) % range + range) % range - 12;
        accumOffset[prevStep] = v;
      }
```
Reemplazar por:
```cpp
      if (prevMode == SM_ACCUM_UP || prevMode == SM_ACCUM_DOWN) {
        int amt  = (int)std::round(params[ACCUM_AMT_PARAM].getValue());
        int clip = (int)std::round(params[ACCUM_CLIP_PARAM].getValue());
        int signedAmt = (prevMode == SM_ACCUM_UP) ? amt : -amt;
        int v = accumOffset[prevStep] + signedAmt;
        static constexpr int ACCUM_RANGE = 25; // -12..+12 inclusive
        v = ((v + 12) % ACCUM_RANGE + ACCUM_RANGE) % ACCUM_RANGE - 12;
        accumOffset[prevStep] = v;
        if (clip > 0) {
          accumGlobalCount++;
          if (accumGlobalCount >= clip) {
            for (int i = 0; i < 16; ++i) accumOffset[i] = 0;
            accumGlobalCount = 0;
          }
        }
      }
```

- [ ] **Step 2: Compilar**

```bash
RACK_DIR=../Rack-SDK make -j$(nproc) 2>&1 | grep -E "error:"
```

- [ ] **Step 3: Commit**

```bash
git add src/UZZ.cpp
git commit -m "feat: accumulator clip logic — global reset every N sums"
```

---

### Task 4: Widget `AccumDisplay` (doble línea st / clip)

**Files:**
- Modify: `src/UZZ.cpp` — justo antes de `struct UZZWidget` (alrededor de línea 1600)

- [ ] **Step 1: Añadir `AccumDisplay` después de `ParamDisplay`**

Localizar el fin del struct `ParamDisplay` (buscar `};` después de `drawLayer` de `ParamDisplay`, alrededor de línea 1507):
```cpp
    nvgText(args.vg, cx, cy, txt.c_str(), nullptr);
  }
};
```
Insertar después (antes de `static NVGcolor panelTextColor()`):
```cpp
// Muestra en dos líneas: semitones (arriba) y clip count (abajo).
struct AccumDisplay : TransparentWidget {
  UZZ* module;

  AccumDisplay(Vec pos, Vec size, UZZ* m) : module(m) {
    box.pos = pos;
    box.size = size;
  }

  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer != 1) return;
    std::shared_ptr<Font> font = APP->window->uiFont;
    if (!font) return;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.5f);
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 180));
    nvgFill(args.vg);

    std::string stTxt   = "--";
    std::string clipTxt = "OFF";
    if (module) {
      int st   = (int)std::round(module->params[UZZ::ACCUM_AMT_PARAM].getValue());
      int clip = (int)std::round(module->params[UZZ::ACCUM_CLIP_PARAM].getValue());
      stTxt   = std::to_string(st) + "st";
      clipTxt = (clip > 0) ? std::to_string(clip) + "x" : "OFF";
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, nvgRGB(0x5D, 0xB7, 0xFF));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    float cx = box.size.x * 0.5f;
    nvgFontSize(args.vg, 8.5f);
    nvgText(args.vg, cx, box.size.y * 0.30f, stTxt.c_str(), nullptr);
    nvgText(args.vg, cx, box.size.y * 0.72f, clipTxt.c_str(), nullptr);
  }
};
```

- [ ] **Step 2: Compilar**

```bash
RACK_DIR=../Rack-SDK make -j$(nproc) 2>&1 | grep -E "error:"
```

- [ ] **Step 3: Commit**

```bash
git add src/UZZ.cpp
git commit -m "feat: add AccumDisplay widget (two-line: st / clip)"
```

---

### Task 5: Layout — 2 Trimpots + AccumDisplay en panel

**Files:**
- Modify: `src/UZZ.cpp:1827-1828` (knob), `src/UZZ.cpp:1844-1845` (display)

- [ ] **Step 1: Reemplazar el knob único por dos Trimpots**

Localizar (alrededor de línea 1827-1828):
```cpp
    addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_CTRL2, yBot), module,
                                             UZZ::ACCUM_AMT_PARAM));
```
Reemplazar por:
```cpp
    addParam(createParamCentered<Trimpot>(Vec(UI::X_CTRL2 - 10.f, yBot), module,
                                          UZZ::ACCUM_AMT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(UI::X_CTRL2 + 10.f, yBot), module,
                                          UZZ::ACCUM_CLIP_PARAM));
```

- [ ] **Step 2: Reemplazar el display de un param por AccumDisplay**

Localizar (alrededor de línea 1844-1845, dentro del bloque `{ const float dW...}`):
```cpp
      addDispAt(xStep8, yBot, UZZ::ACCUM_AMT_PARAM);
```
Reemplazar por:
```cpp
      addChild(new AccumDisplay(
          Vec(xStep8 - dW * .5f, yBot - dH * .5f), Vec(dW, dH), module));
```

- [ ] **Step 3: Compilar limpio**

```bash
RACK_DIR=../Rack-SDK make -j$(nproc) 2>&1 | grep -E "error:|aviso:|warning:" | grep -v "vla-extension"
```
Resultado esperado: sin salida (cero errores y cero warnings).

- [ ] **Step 4: Commit final**

```bash
git add src/UZZ.cpp
git commit -m "feat: accumulator clip — 2 Trimpots + AccumDisplay en panel"
```

---

### Task 6: `formatValue` para tooltip del ACCUM_CLIP_PARAM

El tooltip del param (al hacer hover sobre el knob) usa el label de `configParam`. Ya está configurado en Task 1. No se necesita nada más aquí — Rack muestra automáticamente el valor formateado con snap.

Sin embargo, si se quiere mostrar "OFF" cuando clip=0 en la ventana de módulo, se puede añadir a `ParamDisplay::formatValue`:

- [ ] **Step 1 (opcional): Añadir case a formatValue**

Localizar (alrededor de línea 1474-1477):
```cpp
    case UZZ::ACCUM_AMT_PARAM: {
      int st = (int)std::round(v);
      return std::to_string(st) + "st";
    }
```
Añadir después:
```cpp
    case UZZ::ACCUM_CLIP_PARAM: {
      int n = (int)std::round(v);
      return (n > 0) ? std::to_string(n) + "x" : "OFF";
    }
```

- [ ] **Step 2: Compilar final**

```bash
RACK_DIR=../Rack-SDK make -j$(nproc) 2>&1 | grep -E "error:|aviso:|warning:" | grep -v "vla-extension"
```

- [ ] **Step 3: Commit**

```bash
git add src/UZZ.cpp
git commit -m "feat: formatValue case for ACCUM_CLIP_PARAM"
```
