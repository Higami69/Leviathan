#include "Include.h"
// ------------------------------------ //
#ifndef LEVIATHAN_SCRIPT_EXECUTOR
#include "ScriptExecutor.h"
#endif
using namespace Leviathan;
// ------------------------------------ //
#include "Script/AngelScriptCommon.h"
#include "Iterators/StringIterator.h"

#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptmath/scriptmathcomplex.h>
#include <add_on/scriptmath/scriptmath.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptgrid/scriptgrid.h>

// headers that contains bind able functions //
#include "GUI/GuiScriptInterface.h"

// include various headers that bind functions and classes //
#include "GUI/GuiScriptBind.h"
#include "CommonEngineBind.h"
#include <add_on/scripthelper/scripthelper.h>
#include "add_on/scriptdictionary/scriptdictionary.h"
#include "Application/Application.h"
#include "ScriptNotifiers.h"

using namespace Leviathan::Script;


ScriptExecutor::ScriptExecutor() : engine(NULL), AllocatedScriptModules(){
    
	instance = this;

    // Initialize AngelScript //
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if(engine == NULL){

		Logger::Get()->Error("ScriptExecutor: Init: asCreateScriptEngine failed");
		Logger::Get()->Info("ScriptExecutor: tried to init angelscript version " +
            Convert::ToString(ANGELSCRIPT_VERSION));
		Logger::Get()->Write("Did you use a wrong angelscript version? copy header files to "
            "leviathan/Angelscript/include from your angelscript.zip");
        throw Exception("Failed to init angelscript");
	}

	// set callback to error report function //
	engine->SetMessageCallback(asFUNCTION(ScriptMessageCallback), 0, asCALL_CDECL);

	// math functions //
	RegisterScriptMath(engine);
	RegisterScriptMathComplex(engine);

	// register script string type //
	RegisterStdString(engine);
	RegisterScriptArray(engine, true);
	// register other script extensions //
	RegisterStdStringUtils(engine);

	// register dictionary object //
	RegisterScriptDictionary(engine);
	
	// Register the grid addon //
	RegisterScriptGrid(engine);

	// register global functions and classes //
	if(engine->RegisterGlobalFunction("void Print(string message, bool save = true)", asFUNCTION(Logger::Print),
            asCALL_CDECL) < 0)
    {
		// error abort //
		Logger::Get()->Error(L"ScriptExecutor: Init: AngelScript: RegisterGlobalFunction: failed, line: "+
            Convert::IntToWstring(__LINE__));
        throw Exception("Script bind failed");
	}

	// use various binding functions //

	// binding Event DataStore DataBlock and others //
	if(!BindEngineCommonScriptIterface(engine)){
		// failed //
		Logger::Get()->Error(L"ScriptExecutor: Init: AngelScript: register Engine object things failed");
        throw Exception("Script bind failed");
	}

	// binding TextLabel and other objects to be elements //
	if(!BindGUIObjects(engine)){
		// failed //
		Logger::Get()->Error(L"ScriptExecutor: Init: AngelScript: register GUI object things failed");
        throw Exception("Script bind failed");
	}

	// Bind notifiers //
	if(!RegisterNotifiersWithAngelScript(engine)){
		// failed //
		Logger::Get()->Error(L"ScriptExecutor: Init: AngelScript: register Notifier types failed");
        throw Exception("Script bind failed");
	}


	// bind application specific //
    auto leviathanengine = Engine::GetEngine();

    if(leviathanengine){

        if(!leviathanengine->GetOwningApplication()->InitLoadCustomScriptTypes(engine)){

            Logger::Get()->Error(L"ScriptExecutor: Init: AngelScript: application register failed");
            throw Exception("Script bind failed");
        }        
    }

    ScanAngelScriptTypes();
}
ScriptExecutor::~ScriptExecutor(){

	auto end = AllocatedScriptModules.end();
	for(auto iter = AllocatedScriptModules.begin(); iter != end; ++iter){

		(*iter)->Release();
	}

	// release/delete all modules //
	AllocatedScriptModules.clear();

	// release AngelScript //
	SAFE_RELEASE(engine);

	// release these to stop VLD complains //
	EngineTypeIDS.clear();
	EngineTypeIDSInverted.clear();

    instance = NULL;
    
	if(engine){

        engine->Release();
        engine = NULL;
    }
}

DLLEXPORT ScriptExecutor* Leviathan::ScriptExecutor::Get(){
	return instance;
}

ScriptExecutor* Leviathan::ScriptExecutor::instance = NULL;
// ------------------------------------ //
DLLEXPORT shared_ptr<VariableBlock> Leviathan::ScriptExecutor::RunSetUp(ScriptScript* scriptobject,
    ScriptRunningSetup* parameters)
{
	// Get the ScriptModule for the script //
	ScriptModule* scrptmodule = scriptobject->GetModule();
    
	if(!scrptmodule){
        
		// report error and exit //
		Logger::Get()->Error(L"ScriptExecutor: RunSetUp: trying to run an empty module");
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	// Use the actual running function //
	return RunSetUp(scrptmodule, parameters);
}

DLLEXPORT shared_ptr<VariableBlock> Leviathan::ScriptExecutor::RunSetUp(ScriptModule* scrptmodule,
    ScriptRunningSetup* parameters)
{
    
	if(!scrptmodule){
		Logger::Get()->Error(L"ScriptExecutor: RunSetUp: trying to run without a module");
		return NULL;
	}

	// Load the actual script //
	asIScriptModule* Module = scrptmodule->GetModule();
    
	if(!Module){
        
		// report error and exit //
		Logger::Get()->Error(L"ScriptExecutor: RunSetUp: cannot run an invalid script module: "+
            scrptmodule->GetInfoWstring(), true);
        
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	// Get a function pointer to the start function //
	asIScriptFunction *func = NULL;

	// Get the entry function from the module //
	if(!parameters->FullDeclaration){

		func = Module->GetFunctionByName(parameters->Entryfunction.c_str());
	} else {

		func = Module->GetFunctionByDecl(parameters->Entryfunction.c_str());
	}

	if(!_CheckScriptFunctionPtr(func, parameters, scrptmodule)){
		
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	// Create a running context for the function //
	asIScriptContext* ScriptContext = _GetContextForExecution();

	if(!ScriptContext)
		return shared_ptr<VariableBlock>(new VariableBlock(-1));

	if(!_PrepareContextForPassingParameters(func, ScriptContext, parameters, scrptmodule)){

		_DoneWithContext(ScriptContext);
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}


	// Get the function parameter info //
	FunctionParameterInfo* paraminfo = scrptmodule->GetParamInfoForFunction(func);

	// Pass the parameters //
	if(!_SetScriptParameters(ScriptContext, parameters, scrptmodule, paraminfo)){

		// Failed passing the parameters //
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	// Run the script //

	// Could use a timeout here //
	int retcode = ScriptContext->Execute();

	// Get the return value //
	auto returnvalue = _GetScriptReturnedVariable(retcode, ScriptContext, parameters, func, scrptmodule, paraminfo);

	// Release the context //
	_DoneWithContext(ScriptContext);

	// Return the returned value //
	return returnvalue;
}

DLLEXPORT shared_ptr<VariableBlock> Leviathan::ScriptExecutor::RunSetUp(asIScriptFunction* function,
    ScriptRunningSetup* parameters)
{
	// Find a script module by the name //
    // TODO: find by AngelScript module pointer
	auto locked = GetModuleByAngelScriptName(function->GetModuleName()).lock();

	ScriptModule* scrptmodule = locked ? locked.get(): NULL;
    
	if(!scrptmodule){
        
		// report error and exit //
		if(parameters->ErrorOnNonExistingFunction)
			Logger::Get()->Error(L"ScriptExecutor: RunSetUp: the module is no longer available: "+
                Convert::StringToWstring(function->GetModuleName()));
        
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	if(!_CheckScriptFunctionPtr(function, parameters, scrptmodule)){

		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	// Create a running context for the function //
	asIScriptContext* ScriptContext = _GetContextForExecution();

	if(!ScriptContext)
		return shared_ptr<VariableBlock>(new VariableBlock(-1));

	if(!_PrepareContextForPassingParameters(function, ScriptContext, parameters, scrptmodule)){

		_DoneWithContext(ScriptContext);
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}


	// Get the function parameter info //
	FunctionParameterInfo* paraminfo = scrptmodule->GetParamInfoForFunction(function);

	// Pass the parameters //
	if(!_SetScriptParameters(ScriptContext, parameters, scrptmodule, paraminfo)){

		// Failed passing the parameters //
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	// Run the script //

	// Could use a timeout here //
	int retcode = ScriptContext->Execute();

	// Get the return value //
	auto returnvalue = _GetScriptReturnedVariable(retcode, ScriptContext, parameters, function, scrptmodule, paraminfo);

	// Release the context //
	_DoneWithContext(ScriptContext);

	// Return the returned value //
	return returnvalue;
}
// ------------------------------------ //
bool Leviathan::ScriptExecutor::_SetScriptParameters(asIScriptContext* ScriptContext, ScriptRunningSetup* parameters,
    ScriptModule* scrptmodule, FunctionParameterInfo* paraminfo)
{
	// Get the number of parameters expected //
	int parameterc = paraminfo->ParameterTypeIDS.size();

	// Start passing the parameters provided by the application //
	for(int i = 0; i < parameterc; i++){
		if(i >= (int)parameters->Parameters.size()) // no more parameters //
			break;

		// Try to pass the parameter //
		switch(paraminfo->MatchingDataBlockTypes[i]){
            case DATABLOCK_TYPE_INT:
			{
				int tmpparam = 0;

				if(!parameters->Parameters[i]->ConvertAndAssingToVariable(tmpparam)){

					goto scriptexecutorpassparamsinvalidconversionparam;
				}

				ScriptContext->SetArgDWord(i, tmpparam);
			}
			break;
            case DATABLOCK_TYPE_FLOAT:
			{
				float tmpparam = 0;

				if(!parameters->Parameters[i]->ConvertAndAssingToVariable(tmpparam)){

					goto scriptexecutorpassparamsinvalidconversionparam;
				}

				ScriptContext->SetArgFloat(i, tmpparam);
			}
			break;
            case DATABLOCK_TYPE_BOOL: case DATABLOCK_TYPE_CHAR:
			{
				char tmpparam = 0;

				if(!parameters->Parameters[i]->ConvertAndAssingToVariable(tmpparam)){

					goto scriptexecutorpassparamsinvalidconversionparam;
				}

				ScriptContext->SetArgByte(i, tmpparam);
			}
			break;
            case DATABLOCK_TYPE_STRING:
            {

                string* varpointer = static_cast<string*>(*parameters->Parameters[i]);

                Logger::Get()->Write("Script passing string parameter: "+*varpointer);

                ScriptContext->SetArgObject(i, varpointer);
                
            }
            break;
            case DATABLOCK_TYPE_WSTRING:
			{
				// save as a string that can be retrieved //
				// not done
				goto scriptexecutorpassparamsinvalidconversionparam;
			}
			break;
            case DATABLOCK_TYPE_VOIDPTR:
			{
				// we need to make sure that script object name matches engine object name //
				if(paraminfo->ParameterDeclarations[i] != parameters->Parameters[i]->GetName()){
					// non matching pointer types //
					Logger::Get()->Error(L"Mismatching ptr types, comparing "+paraminfo->ParameterDeclarations[i]+
                        L" to passed type of "+parameters->Parameters[i]->GetName());
                    
				} else {
                    
					// types match, we can pass in the raw pointer //
					void* ptrtostuff = (void*)(*parameters->Parameters[i]);
					ScriptContext->SetArgAddress(i, ptrtostuff);
				}
			}
			break;
            default:
                goto scriptexecutorpassparamsinvalidconversionparam;
		}

		continue;

scriptexecutorpassparamsinvalidconversionparam:


		Logger::Get()->Error(L"ScriptExecutor: RunScript: pass parameters failed func: "+
            Convert::StringToWstring(parameters->Entryfunction)+L" param number: "+Convert::IntToWstring(i)+L" in: "+
            scrptmodule->GetInfoWstring(), true);
		return false;
	}

	// It didn't fail //
	return true;
}

shared_ptr<VariableBlock> Leviathan::ScriptExecutor::_GetScriptReturnedVariable(int retcode, asIScriptContext*
    ScriptContext, ScriptRunningSetup* parameters, asIScriptFunction* func, ScriptModule* scrptmodule,
    FunctionParameterInfo* paraminfo)
{
	// Check the return type //
	if(retcode != asEXECUTION_FINISHED){
		// something went wrong //

		// The execution didn't finish as we had planned. Determine why.
		if(retcode == asEXECUTION_ABORTED){
			// code took too long //
		} else if(retcode == asEXECUTION_EXCEPTION){
			// script caused an exception //
			const asIScriptFunction* exceptionfunc = ScriptContext->GetExceptionFunction();

			int linenumber = ScriptContext->GetExceptionLineNumber();

			Logger::Get()->Error(L"[SCRIPT] [EXCEPTION] "+Convert::StringToWstring(ScriptContext->GetExceptionString())+
                L", function: "+Convert::ToWstring(func->GetDeclaration())+L"\n\t in "+Convert::ToWstring(
                    exceptionfunc->GetScriptSectionName())+L"("+Convert::ToWstring(linenumber)+L") "+
                scrptmodule->GetInfoWstring());


			PrintAdditionalExcept(ScriptContext);
		}
		
		return shared_ptr<VariableBlock>(new VariableBlock(-1));
	}

	// Successfully executed, try to fetch return value //
	if(paraminfo->ReturnTypeID != 0){
		// return type isn't void //
		switch(paraminfo->ReturnMatchingDataBlock){
            case DATABLOCK_TYPE_INT:
			{
				return shared_ptr<VariableBlock>(new VariableBlock(new IntBlock(ScriptContext->GetReturnDWord())));
			}
			break;
            case DATABLOCK_TYPE_FLOAT:
			{
				return shared_ptr<VariableBlock>(new VariableBlock(new FloatBlock(ScriptContext->GetReturnFloat())));
			}
			break;
            case DATABLOCK_TYPE_BOOL:
			{
				return shared_ptr<VariableBlock>(new VariableBlock(new BoolBlock(ScriptContext->GetReturnByte() != 0)));
			}
			break;
            case DATABLOCK_TYPE_CHAR:
			{
				return shared_ptr<VariableBlock>(new VariableBlock(new CharBlock(ScriptContext->GetReturnByte())));
			}
			break;
            case DATABLOCK_TYPE_STRING:
            {

                // TODO: check do we need to delete this
                
                string* varpointer = reinterpret_cast<string*>(ScriptContext->GetReturnObject());

                if(varpointer){

                    return make_shared<VariableBlock>(new StringBlock(*varpointer));
                    
                } else {

                    return make_shared<VariableBlock>(new VoidPtrBlock((void*)nullptr));
                }
            }
            break;
		}

		Logger::Get()->Info(L"[SCRIPT] return type not supported "+paraminfo->ReturnTypeDeclaration);
        
		return shared_ptr<VariableBlock>(new VariableBlock(string("Return type not supported")));
	}

	// No return value //
	return NULL;
}

bool Leviathan::ScriptExecutor::_CheckScriptFunctionPtr(asIScriptFunction* func, ScriptRunningSetup* parameters,
    ScriptModule* scrptmodule)
{
	// Check is it NULL //
	if(func == NULL){
		// Set exists state //
		parameters->ScriptExisted = false;

		// Check should we print an error //
		if(parameters->PrintErrors && parameters->ErrorOnNonExistingFunction){
            
			Logger::Get()->Error(L"ScriptExecutor: RunScript: Could not find starting function: "+
                Convert::StringToWstring(parameters->Entryfunction)+L" in: "+scrptmodule->GetInfoWstring(), true);
            
			scrptmodule->PrintFunctionsInModule();
		}

		// Not valid //
		return false;
	}

	// Set exists state //
	parameters->ScriptExisted = true;

	return true;
}


bool Leviathan::ScriptExecutor::_PrepareContextForPassingParameters(asIScriptFunction* func, asIScriptContext*
    ScriptContext, ScriptRunningSetup* parameters, ScriptModule* scrptmodule)
{
	if(ScriptContext->Prepare(func) < 0){
        
		Logger::Get()->Error(L"ScriptExecutor: RunScript: prepare context failed, func: "+Convert::StringToWstring(
                parameters->Entryfunction)+L" in: "+scrptmodule->GetInfoWstring(), true);
        
		return false;
	}

	return true;
}

// ------------------------------------ //
asIScriptContext* Leviathan::ScriptExecutor::_GetContextForExecution(){

    // TODO: pool context and detect already acive context and push state and use that
	asIScriptContext* ScriptContext = engine->CreateContext();

	if(!ScriptContext){

		Logger::Get()->Error(L"ScriptExecutor: RunScript: Failed to create a context ", true);
		return NULL;
	}

	return ScriptContext;
}

void Leviathan::ScriptExecutor::_DoneWithContext(asIScriptContext* context){
	context->Release();
}
// ------------------------------------ //
DLLEXPORT weak_ptr<ScriptModule> Leviathan::ScriptExecutor::GetModule(const int &ID){
	// loop modules and return a ptr to matching id //
	for(size_t i = 0; i < AllocatedScriptModules.size(); i++){
		if(AllocatedScriptModules[i]->GetID() == ID)
			return AllocatedScriptModules[i];
	}
	return shared_ptr<ScriptModule>(NULL);
}

DLLEXPORT weak_ptr<ScriptModule> Leviathan::ScriptExecutor::GetModuleByAngelScriptName(const char* nameofmodule){
	// Find a matching name //
	string module(nameofmodule);

    // TODO: check could this be checked by comparing pointers
	for(size_t i = 0; i < AllocatedScriptModules.size(); i++){
		if(AllocatedScriptModules[i]->GetModuleName() == module)
			return AllocatedScriptModules[i];
	}

	return shared_ptr<ScriptModule>(NULL);
}
// ------------------------------------ //
DLLEXPORT weak_ptr<ScriptModule> Leviathan::ScriptExecutor::CreateNewModule(const wstring &name, const string &source,
    const int &modulesid /*= IDFactory::GetID()*/)
{
	// create new module to a smart pointer //
	shared_ptr<ScriptModule> tmpptr(new ScriptModule(engine, name, modulesid, source));

	// add to vector and return //
	AllocatedScriptModules.push_back(tmpptr);
	return tmpptr;
}

DLLEXPORT void Leviathan::ScriptExecutor::DeleteModule(ScriptModule* ptrtomatch){
	// find module based on pointer and remove //
	for(size_t i = 0; i < AllocatedScriptModules.size(); i++){
		if(AllocatedScriptModules[i].get() == ptrtomatch){

			AllocatedScriptModules[i]->Release();
			// remove //
			AllocatedScriptModules.erase(AllocatedScriptModules.begin()+i);
			return;
		}
	}
}

DLLEXPORT bool Leviathan::ScriptExecutor::DeleteModuleIfNoExternalReferences(int ID){
	// Find based on the id //
	for(size_t i = 0; i < AllocatedScriptModules.size(); i++){
		if(AllocatedScriptModules[i]->GetID() == ID){
			// Check reference count //
			if(AllocatedScriptModules[i].use_count() != 1){
				// Other references exist //
				return false;
			}

			AllocatedScriptModules[i]->Release();

			// remove //
			AllocatedScriptModules.erase(AllocatedScriptModules.begin()+i);
			return true;
		}
	}
	// Nothing found //
	return false;
}
// ------------------------------------ //
void Leviathan::ScriptExecutor::PrintAdditionalExcept(asIScriptContext *ctx){
	// Print callstack as additional information //
	Logger::Get()->Write(L"// ------------------ CallStack ------------------ //\n");
	// Loop the stack starting from the frame below the current function (actually might be nice to print the top frame too) //
	for(UINT n = 0; n < ctx->GetCallstackSize(); n++){
		// Get the function object //
		const asIScriptFunction* function = ctx->GetFunction(n);
		// If we function doesn't exist this frame is used internally by the script engine //
		if(function){
			// Check function type //
			if(function->GetFuncType() == asFUNC_SCRIPT){
				// Print info about the script function //
				Logger::Get()->Write(L"\t> "+Convert::StringToWstring(function->GetScriptSectionName())+L" ("+Convert::ToWstring(ctx->GetLineNumber(n))
					+L"): "+Convert::StringToWstring(function->GetDeclaration())+L"\n");
			} else {
				// Info about the application functions //
				// The context is being reused by the application for a nested call
				Logger::Get()->Write(L"\t> {...Application...}: "+Convert::StringToWstring(function->GetDeclaration())+L"\n");
			}
		} else {
			// The context is being reused by the script engine for a nested call
			Logger::Get()->Write(L"\t> {...Script internal...}\n");
		}
	}
}
// ------------------------------------ //
DLLEXPORT void Leviathan::ScriptExecutor::ScanAngelScriptTypes(){
	if(EngineTypeIDS.empty()){
		// put basic types //
		EngineTypeIDS.insert(make_pair(engine->GetTypeIdByDecl("int"), L"int"));
		EngineTypeIDS.insert(make_pair(engine->GetTypeIdByDecl("float"), L"float"));
		EngineTypeIDS.insert(make_pair(engine->GetTypeIdByDecl("bool"), L"bool"));
		EngineTypeIDS.insert(make_pair(engine->GetTypeIdByDecl("string"), L"string"));
		EngineTypeIDS.insert(make_pair(engine->GetTypeIdByDecl("void"), L"void"));
	}
	// call some callbacks //
	RegisterGUIScriptTypeNames(engine, EngineTypeIDS);
	RegisterEngineScriptTypes(engine, EngineTypeIDS);
	RegisterNotifierTypesWithAngelScript(engine, EngineTypeIDS);

    
    auto leviathanengine = Engine::GetEngine();
    if(leviathanengine){
        
        leviathanengine->GetOwningApplication()->RegisterCustomScriptTypes(engine, EngineTypeIDS);
    }

	// invert the got list, since it should be final //
	for(auto iter = EngineTypeIDS.begin(); iter != EngineTypeIDS.end(); ++iter){

		EngineTypeIDSInverted.insert(make_pair(iter->second, iter->first));
	}
}

std::map<wstring, int> Leviathan::ScriptExecutor::EngineTypeIDSInverted;

std::map<int, wstring> Leviathan::ScriptExecutor::EngineTypeIDS;

