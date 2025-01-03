#include "PCH.h"
#include "CVarInternal.h"
#include "Parser.h"


CVarSystem* CVarSystem::Get()
{
  static CVarSystem system{};
  return &system;
}

CVarSystem::CVarSystem()
{
  storage = new CVarSystemStorage;
}

CVarSystem::~CVarSystem()
{
  delete storage;
}

CVarParameters* CVarSystem::GetCVarParams(const char* name)
{
  if (auto it = storage->cvarParameters.find(hashed_string(name)); it != storage->cvarParameters.end())
  {
    return &it->second;
  }
  return nullptr;
}



CVarParameters* CVarSystem::InitCVar(const char* name, const char* description, CVarFlags flags)
{
  std::unique_lock lck(storage->mutex);
  CVarParameters& params = storage->cvarParameters[hashed_string(name)];
  params.index = -1;
  params.name = name;
  params.description = description;
  params.flags = flags;
  return &params;
}


// TODO: return an error code
bool CVarSystem::SetCVarParse(const char* name, const char* args)
{
  CVarParameters* params = GetCVarParams(name);
  if (params == nullptr) return false;

  CmdParser parser(args);
  CmdAtom atom = parser.NextAtom();

  switch (params->type)
  {
  case CVarType::FLOAT:
  {
    auto* f = std::get_if<cvar_float>(&atom);
    if (f) return SetCVar<cvar_float>(name, *f);
    break;
  }
  case CVarType::STRING:
  {
    auto* s = std::get_if<std::string>(&atom);
    if (s) return SetCVar<cvar_string>(name, s->c_str());
    break;
  }
  case CVarType::VEC3:
  {
    auto* v = std::get_if<cvar_vec3>(&atom);
    if (v) return SetCVar<cvar_vec3>(name, *v);
    break;
  }
  }

  return false;
}



CVarParameters* CVarSystem::RegisterCVar(const char* name, const char* description,
  cvar_float defaultValue, cvar_float minValue, cvar_float maxValue, CVarFlags flags,
  OnChangeCallback<cvar_float> callback)
{
  auto params = InitCVar(name, description, flags);
  params->type = CVarType::FLOAT;
  storage->floatCVars.AddCVar(defaultValue, params, callback, minValue, maxValue);
  return params;
}

CVarParameters* CVarSystem::RegisterCVar(const char* name, const char* description,
  const char* defaultValue, CVarFlags flags, OnChangeCallback<const char*> callback)
{
  auto params = InitCVar(name, description, flags);
  params->type = CVarType::STRING;
  storage->stringCVars.AddCVar(defaultValue, params, callback);
  return params;
}

CVarParameters* CVarSystem::RegisterCVar(const char* name, const char* description,
  cvar_vec3 defaultValue, cvar_vec3 minValue, cvar_vec3 maxValue, CVarFlags flags,
  OnChangeCallback<cvar_vec3> callback)
{
  auto params = InitCVar(name, description, flags);
  params->type = CVarType::VEC3;
  storage->vec3CVars.AddCVar(defaultValue, params, callback, minValue, maxValue);
  return params;
}



template<>
cvar_float CVarSystem::GetCVar(const char* name)
{
  std::shared_lock lck(storage->mutex);
  if (auto* params = GetCVarParams(name))
  {
    return storage->floatCVars.cvars[params->index].current;
  }
  return 0;
}

template<>
const char* CVarSystem::GetCVar(const char* name)
{
  std::shared_lock lck(storage->mutex);
  if (auto* params = GetCVarParams(name))
  {
    return storage->stringCVars.cvars[params->index].current.c_str();
  }
  return "";
}

template<>
cvar_vec3 CVarSystem::GetCVar(const char* name)
{
  std::shared_lock lck(storage->mutex);
  if (auto* params = GetCVarParams(name))
  {
    return storage->vec3CVars.cvars[params->index].current;
  }
  return cvar_vec3{};
}



template<>
bool CVarSystem::SetCVar(const char* name, cvar_float value)
{
  std::unique_lock lck(storage->mutex);
  if (auto* params = GetCVarParams(name))
  {
    auto& cvar = storage->floatCVars.cvars[params->index];
    value = glm::clamp(value, cvar.min, cvar.max);
    if (cvar.callback) cvar.callback(name, value);
    cvar.current = value;
    return true;
  }
  return false;
}

template<>
bool CVarSystem::SetCVar(const char* name, const char* value)
{
  std::unique_lock lck(storage->mutex);
  if (auto* params = GetCVarParams(name))
  {
    auto& cvar = storage->stringCVars.cvars[params->index];
    if (cvar.callback) cvar.callback(name, value);
    cvar.current = value;
    return true;
  }
  return false;
}

template<>
bool CVarSystem::SetCVar(const char* name, cvar_vec3 value)
{
  std::unique_lock lck(storage->mutex);
  if (auto* params = GetCVarParams(name))
  {
    auto& cvar = storage->vec3CVars.cvars[params->index];
    value = glm::clamp(value, cvar.min, cvar.max);
    if (cvar.callback) cvar.callback(name, value);
    cvar.current = value;
    return true;
  }
  return false;
}



AutoCVar<cvar_float>::AutoCVar(const char* name,
  const char* description,
  cvar_float defaultValue,
  cvar_float minValue,
  cvar_float maxValue,
  CVarFlags flags,
  OnChangeCallback<cvar_float> callback)
{
  auto* params = CVarSystem::Get()->RegisterCVar(name, description, defaultValue, minValue, maxValue, flags, callback);
  AutoCVarBase::index = params->index;
}

template<>
AutoCVar<const char*>::AutoCVar(const char* name,
  const char* description,
  const char* defaultValue,
  CVarFlags flags,
  OnChangeCallback<const char*> callback)
{
  auto* params = CVarSystem::Get()->RegisterCVar(name, description, defaultValue, flags, callback);
  index = params->index;
}

AutoCVar<cvar_vec3>::AutoCVar(const char* name,
  const char* description,
  cvar_vec3 defaultValue,
  cvar_vec3 minValue,
  cvar_vec3 maxValue,
  CVarFlags flags,
  OnChangeCallback<cvar_vec3> callback)
{
  auto* params = CVarSystem::Get()->RegisterCVar(name, description, defaultValue, minValue, maxValue, flags, callback);
  index = params->index;
}



template<>
cvar_float AutoCVarBase<cvar_float>::Get()
{
  return CVarSystem::Get()->storage->floatCVars.cvars[index].current;
}

template<>
const char* AutoCVarBase<const char*>::Get()
{
  return CVarSystem::Get()->storage->stringCVars.cvars[index].current.c_str();
}

template<>
cvar_vec3 AutoCVarBase<cvar_vec3>::Get()
{
  return CVarSystem::Get()->storage->vec3CVars.cvars[index].current;
}



template<>
void AutoCVarBase<cvar_float>::Set(cvar_float value)
{
  auto& cvar = CVarSystem::Get()->storage->floatCVars.cvars[index];
  CVarSystem::Get()->SetCVar(cvar.parameters->name.c_str(), value);
}

template<>
void AutoCVarBase<const char*>::Set(const char* value)
{
  auto& cvar = CVarSystem::Get()->storage->floatCVars.cvars[index];
  CVarSystem::Get()->SetCVar(cvar.parameters->name.c_str(), value);
}

template<>
void AutoCVarBase<cvar_vec3>::Set(cvar_vec3 value)
{
  auto& cvar = CVarSystem::Get()->storage->floatCVars.cvars[index];
  CVarSystem::Get()->SetCVar(cvar.parameters->name.c_str(), value);
}