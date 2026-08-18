#pragma once
#include "CommonWidgets.hpp"

// Piezas de panel propias de ATEK303, encima del juego común de Animatek.

// Rótulo de sección: el texto centrado con una línea a cada lado. Separa bloques sin
// meter una caja alrededor.
struct SectionLabel : TransparentWidget {
	std::string text;
	float fontSize = 7.f;

	SectionLabel(const char* t, Vec pos, Vec size) : text(t) {
		box.pos = pos;
		box.size = size;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<Font> font = APP->window->uiFont;
		if (!font)
			return;
		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, font->handle);

		float bounds[4];
		nvgTextBounds(args.vg, 0.f, 0.f, text.c_str(), NULL, bounds);
		const float half = 0.5f * (bounds[2] - bounds[0]) + 4.f;
		const float cx = box.size.x * 0.5f;
		const float cy = box.size.y * 0.5f;

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0.f, cy);
		nvgLineTo(args.vg, cx - half, cy);
		nvgMoveTo(args.vg, cx + half, cy);
		nvgLineTo(args.vg, box.size.x, cy);
		nvgStrokeColor(args.vg, AnimatekUI::panelSeparatorColor(110));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);

		nvgFillColor(args.vg, AnimatekUI::panelTextColor());
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, cx, cy, text.c_str(), NULL);
	}
};

// Caja de grupo: rectángulo redondeado con el título partiendo el borde de arriba. El
// borde se dibuja como un trazo que se salta el hueco del texto, así no hace falta
// tapar la línea con un parche del color del panel (que cambia con el tema).
struct GroupBox : TransparentWidget {
	std::string text;          // vacío = caja sin título, el borde va entero
	float fontSize = 7.f;
	float radius = 2.2f;

	GroupBox(const char* t, Vec pos, Vec size) : text(t) {
		box.pos = pos;
		box.size = size;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<Font> font = APP->window->uiFont;
		if (!font)
			return;
		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, font->handle);

		const float w = box.size.x, h = box.size.y, r = radius;
		const float cx = w * 0.5f;
		float half = 0.f;
		if (!text.empty()) {
			float bounds[4];
			nvgTextBounds(args.vg, 0.f, 0.f, text.c_str(), NULL, bounds);
			half = 0.5f * (bounds[2] - bounds[0]) + 3.5f;
		}

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, cx + half, 0.f);
		nvgLineTo(args.vg, w - r, 0.f);
		nvgArcTo(args.vg, w, 0.f, w, r, r);
		nvgLineTo(args.vg, w, h - r);
		nvgArcTo(args.vg, w, h, w - r, h, r);
		nvgLineTo(args.vg, r, h);
		nvgArcTo(args.vg, 0.f, h, 0.f, h - r, r);
		nvgLineTo(args.vg, 0.f, r);
		nvgArcTo(args.vg, 0.f, 0.f, r, 0.f, r);
		nvgLineTo(args.vg, cx - half, 0.f);
		nvgStrokeColor(args.vg, AnimatekUI::panelSeparatorColor(95));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);

		if (text.empty())
			return;
		nvgFillColor(args.vg, AnimatekUI::panelTextColor());
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, cx, 0.f, text.c_str(), NULL);
	}
};
