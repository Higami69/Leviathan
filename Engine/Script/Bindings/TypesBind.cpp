// ------------------------------------ //
#include "TypesBind.h"

#include "Common/Types.h"

using namespace Leviathan;
// ------------------------------------ //

// Proxies etc.
// ------------------------------------ //
// Float3
void Float3ConstructorProxy(void* memory){
	new(memory) Float3();
}

void Float3ConstructorProxyAll(void* memory, float x, float y, float z){
	new(memory) Float3(x, y, z);
}

void Float3ConstructorProxySingle(void* memory, float all){
	new(memory) Float3(all);
}

void Float3ConstructorProxyCopy(void* memory, const Float3 &other){
	new(memory) Float3(other);
}

void Float3DestructorProxy(void* memory){
	reinterpret_cast<Float3*>(memory)->~Float3();
}
// ------------------------------------ //
// Float4
void Float4ConstructorProxy(void* memory){
	new(memory) Float4();
}

void Float4ConstructorProxyAll(void* memory, float x, float y, float z, float w){
	new(memory) Float4(x, y, z, w);
}

void Float4ConstructorProxySingle(void* memory, float all){
	new(memory) Float4(all);
}

void Float4ConstructorProxyCopy(void* memory, const Float4 &other){
	new(memory) Float4(other);
}

void Float4DestructorProxy(void* memory){
	reinterpret_cast<Float4*>(memory)->~Float4();
}



// ------------------------------------ //
// Start of the actual bind
namespace Leviathan{

bool BindFloat3(asIScriptEngine* engine){

	if(engine->RegisterObjectType("Float3", sizeof(Float3), asOBJ_VALUE |
            asGetTypeTraits<Float3>() | asOBJ_APP_CLASS_ALLFLOATS) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
	if(engine->RegisterObjectBehaviour("Float3", asBEHAVE_CONSTRUCT, "void f()",
            asFUNCTION(Float3ConstructorProxy),
            asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
	if(engine->RegisterObjectBehaviour("Float3", asBEHAVE_CONSTRUCT, "void f(float value)",
            asFUNCTION(Float3ConstructorProxySingle), asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
	if(engine->RegisterObjectBehaviour("Float3", asBEHAVE_CONSTRUCT,
            "void f(float x, float y, float z)",
            asFUNCTION(Float3ConstructorProxyAll), asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
	if(engine->RegisterObjectBehaviour("Float3", asBEHAVE_CONSTRUCT,
            "void f(const Float3 &in other)",
            asFUNCTION(Float3ConstructorProxyCopy), asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
	if(engine->RegisterObjectBehaviour("Float3", asBEHAVE_DESTRUCT, "void f()",
            asFUNCTION(Float3DestructorProxy),
            asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
	// Operators //
	if(engine->RegisterObjectMethod("Float3", "Float3& opAssign(const Float3 &in other)",
            asMETHODPR(Float3, operator=, (const Float3&), Float3&), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectMethod("Float3", "Float3 opAdd(const Float3 &in other) const",
            asMETHODPR(Float3, operator+, (const Float3&) const, Float3), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectMethod("Float3", "Float3 opSub(const Float3 &in other) const",
            asMETHODPR(Float3, operator-, (const Float3&) const, Float3), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectMethod("Float3", "Float3 opMul(float multiply) const",
            asMETHODPR(Float3, operator*, (float) const, Float3), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectMethod("Float3", "Float3 Normalize() const",
            asMETHOD(Float3, Normalize),
            asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectMethod("Float3", "float HAddAbs()", asMETHOD(Float3, HAddAbs),
            asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}

    // Direct access
    if(engine->RegisterObjectProperty("Float3", "float X", asOFFSET(Float3, X)) < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectProperty("Float3", "float Y", asOFFSET(Float3, Y)) < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectProperty("Float3", "float Z", asOFFSET(Float3, Z)) < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    return true;
}
// ------------------------------------ //
bool BindFloat4(asIScriptEngine* engine){

    // Float4
    if(engine->RegisterObjectType("Float4", sizeof(Float4), asOBJ_VALUE |
            asGetTypeTraits<Float4>() | asOBJ_APP_CLASS_ALLFLOATS) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectBehaviour("Float4", asBEHAVE_CONSTRUCT, "void f()",
            asFUNCTION(Float4ConstructorProxy),
            asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectBehaviour("Float4", asBEHAVE_CONSTRUCT, "void f(float value)",
            asFUNCTION(Float4ConstructorProxySingle), asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectBehaviour("Float4", asBEHAVE_CONSTRUCT,
            "void f(float x, float y, float z, float w)",
            asFUNCTION(Float4ConstructorProxyAll), asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectBehaviour("Float4", asBEHAVE_CONSTRUCT,
            "void f(const Float4 &in other)",
            asFUNCTION(Float4ConstructorProxyCopy), asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectBehaviour("Float4", asBEHAVE_DESTRUCT, "void f()",
            asFUNCTION(Float4DestructorProxy),
            asCALL_CDECL_OBJFIRST) < 0)
    {
		ANGELSCRIPT_REGISTERFAIL;
	}
    
    // Operators //
    if(engine->RegisterObjectMethod("Float4", "Float4& opAssign(const Float4 &in other)",
            asMETHODPR(Float4, operator=, (const Float4&), Float4&), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}
    
    if(engine->RegisterObjectMethod("Float4", "Float4 opAdd(const Float4 &in other) const",
            asMETHODPR(Float4, operator+, (const Float4&) const, Float4), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}

    if(engine->RegisterObjectMethod("Float4", "Float4 opSub(const Float4 &in other) const",
            asMETHODPR(Float4, operator-, (const Float4&) const, Float4), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}
    
	if(engine->RegisterObjectMethod("Float4", "Float4 opMul(float multiply) const",
            asMETHODPR(Float4, operator*, (float) const, Float4), asCALL_THISCALL) < 0)
	{
		ANGELSCRIPT_REGISTERFAIL;
	}

    // Direct access
    if(engine->RegisterObjectProperty("Float4", "float X", asOFFSET(Float4, X)) < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectProperty("Float4", "float Y", asOFFSET(Float4, Y)) < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectProperty("Float4", "float Z", asOFFSET(Float4, Z)) < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectProperty("Float4", "float W", asOFFSET(Float4, W)) < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    return true;
}
// ------------------------------------ //
bool BindTypeDefs(asIScriptEngine* engine){

    if(engine->RegisterTypedef("ObjectID", "int") < 0){

        ANGELSCRIPT_REGISTERFAIL;
    }

    return true;
}
}

// ------------------------------------ //
// Main bind function
bool Leviathan::BindTypes(asIScriptEngine* engine){

	// Register common float types //
    if(!BindFloat3(engine))
        return false;
    
    if(!BindFloat4(engine))
        return false;

    if(!BindTypeDefs(engine))
        return false;

    return true;
}

void Leviathan::RegisterTypes(asIScriptEngine* engine, std::map<int, std::string> &typeids){
    
	typeids.insert(std::make_pair(engine->GetTypeIdByDecl("Float3"), "Float3"));
    typeids.insert(std::make_pair(engine->GetTypeIdByDecl("Float4"), "Float4"));
}

