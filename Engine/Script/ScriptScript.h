#ifndef LEVIATHAN_SCRIPT_SCRIPT
#define LEVIATHAN_SCRIPT_SCRIPT
// ------------------------------------ //
#ifndef LEVIATHAN_DEFINE
#include "Define.h"
#endif
// ------------------------------------ //
// ---- includes ---- //
#include "ScriptModule.h"

namespace Leviathan{

	// holds a reference of script module //
	class ScriptScript : public Object{
	public:
		DLLEXPORT ScriptScript(weak_ptr<ScriptModule> wptr);
		DLLEXPORT ScriptScript(const int &MID, weak_ptr<ScriptModule> wptr);
		DLLEXPORT ScriptScript(const ScriptScript &other);
		DLLEXPORT ~ScriptScript();

		//! \brief Returns an unsafe pointer to the module
		//! \deprecated GetModuleSafe should be used instead, will probably be around for a while
		DLLEXPORT inline ScriptModule* GetModule(){
			return ScriptsModule.lock().get();
		}


		//! \brief Locks the weak pointer and returns the shared_ptr
		//! \note This should be used instead of GetModule if some methods are called on the pointer
		DLLEXPORT inline shared_ptr<ScriptModule> GetModuleSafe(){

			return ScriptsModule.lock();
		}



	private:
		// reference to module //
		weak_ptr<ScriptModule> ScriptsModule;
		int ModuleID;
	};

}
#endif