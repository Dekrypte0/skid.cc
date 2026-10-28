#include "CGameEntitySystem.h"
#include "..\..\hooks\hooks.h"
#include "..\..\utils\memory\memsafe\memsafe.h"
#include "..\..\utils\console\console.h"
#include <Windows.h>

void* CGameEntitySystem::
GetEntityByIndex(int nIndex) {
	if (!Mem::
ValidEntityIndex(nIndex))
		return nullptr;

	// The interface wrapper normally supplies this.  If it is stale (or the
	// interface name disappeared), use the current-build entity-list global
	// resolved from the supplied RIP-relative signature.  Try the preferred
	// object first and the global object second, without recursively calling
	// through the public Get() template.
	CGameEntitySystem* systems[2]{ this, H::
ClientEntitySystemFromGlobal() };
	for (int si = 0; si < 2; ++si) {
		CGameEntitySystem* system = systems[si];
		if (!Mem::
IsUserPtr(system) || (si == 1 && system == systems[0]))
			continue;

		// Prefer Valve's resolver when its signature is present.  The direct
		// identity-list walk below is a build-stable fallback; otherwise one
		// moved GetBaseEntity pattern makes every ESP/cache lookup return null.
		if (H::
	ogGetBaseEntity) {
			void* result = nullptr;
			__try {
				result = H::
		ogGetBaseEntity(system, nIndex);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				TW_SEH_CATCH("entitySys.getBaseEntity");
			}
			if (Mem::
		ValidEntity(result))
				return result;
		}

		// CGameEntitySystem stores 0x200-entry identity chunks at +0x10 with a
		// 0x70-byte identity stride. get_entity() verifies the handle index
		// before exposing the object, so this path is safe against recycled or
		// empty slots.
		void* direct = nullptr;
		__try {
			direct = reinterpret_cast<void*>(system->get_entity(nIndex));
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			TW_SEH_CATCH("entitySys.directLookup");
		}
		if (Mem::
		ValidEntity(direct))
			return direct;
	}
	return nullptr;
}

bool CGameEntitySystem::
MatchHandleSerial(void* entity, const CBaseHandle& expected) noexcept
{
	if (!entity || !expected.valid())
		return false;
	CBaseHandle actual{};
	__try {
		actual = reinterpret_cast<CEntityInstance*>(entity)->handle();
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		TW_SEH_CATCH("entitySys.matchHandleSerial");
		return false;
	}
	return actual.valid()
		&& actual.index() == expected.index()
		&& actual.serial_number() == expected.serial_number();
}
