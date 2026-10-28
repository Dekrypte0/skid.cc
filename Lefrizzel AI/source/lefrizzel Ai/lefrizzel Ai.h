#pragma once
#include "config/config.h"

#include "hooks/hooks.h"
#include "renderer/renderer.h"
#include "utils/schema/schema.h"
#include "interfaces/interfaces.h"
class LefrizzelAi {
public:
	// Returns true when schema/interfaces and the renderer are ready. Individual
	// pattern hooks may be partial; Present/visual features stay available while
	// the affected combat hook fails closed.
	bool init(HWND& window, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* mainRenderTargetView);
	void shutdown() noexcept;

	Schema schema;
	Renderer renderer;


	H::
Hooks hooks;
	I::
Interfaces interfaces;

};
