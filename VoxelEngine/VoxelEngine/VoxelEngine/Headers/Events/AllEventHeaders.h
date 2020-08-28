#ifndef EVENT_COUNT
#define EVENT_COUNT 8
#endif

#ifdef FACTORY_RUNNING

#include "../Headers/Factory.h"
#include "DestroyEvent.h"
#include "DrawEvent.h"
#include "InitEvent.h"
#include "RenderEvent.h"
#include "StubEvent.h"
#include "TestingEvent.h"
#include "TraceEvent.h"
#include "UpdateEvent.h"


void RegisterEvents()
{
Factory::EventPropertyMap["DestroyEvent"] = std::vector<PropertyID>({
  });
Factory::EventPropertyMap["DrawEvent"] = std::vector<PropertyID>({
  });
Factory::EventPropertyMap["InitEvent"] = std::vector<PropertyID>({
  });
Factory::EventPropertyMap["RenderEvent"] = std::vector<PropertyID>({
  });
Factory::EventPropertyMap["StubEvent"] = std::vector<PropertyID>({
  });
Factory::EventPropertyMap["TestingEvent"] = std::vector<PropertyID>({
  });
Factory::EventPropertyMap["TraceEvent"] = std::vector<PropertyID>({
  });
Factory::EventPropertyMap["UpdateEvent"] = std::vector<PropertyID>({
  PropertyID("dt", UpdateEvent::dt_id, offsetof(UpdateEvent, dt), sizeof(Float)),
  PropertyID("elapsedTime", UpdateEvent::elapsedTime_id, offsetof(UpdateEvent, elapsedTime), sizeof(Float)),
  });
}

inline std::unique_ptr<Event> Event0() { return std::move(DestroyEvent::RegisterDestroyEvent()); }
inline std::unique_ptr<Event> Event1() { return std::move(DrawEvent::RegisterDrawEvent()); }
inline std::unique_ptr<Event> Event2() { return std::move(InitEvent::RegisterInitEvent()); }
inline std::unique_ptr<Event> Event3() { return std::move(RenderEvent::RegisterRenderEvent()); }
inline std::unique_ptr<Event> Event4() { return std::move(StubEvent::RegisterStubEvent()); }
inline std::unique_ptr<Event> Event5() { return std::move(TestingEvent::RegisterTestingEvent()); }
inline std::unique_ptr<Event> Event6() { return std::move(TraceEvent::RegisterTraceEvent()); }
inline std::unique_ptr<Event> Event7() { return std::move(UpdateEvent::RegisterUpdateEvent()); }


#endif // !FACTORY_RUNNING