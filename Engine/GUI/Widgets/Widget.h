// Leviathan Game Engine
// Copyright (c) 2012-2018 Henri Hyyryläinen
#pragma once
#include "Define.h"
// ------------------------------------ //
#include "Common/ReferenceCounted.h"

namespace Leviathan { namespace GUI {

class WidgetContainer;

//! \brief Base class for all Leviathan GUI widgets
class Widget : public ReferenceCounted {
    friend WidgetContainer;

protected:
    DLLEXPORT Widget();

public:
    DLLEXPORT virtual ~Widget();

    DLLEXPORT virtual void Tick() = 0;

protected:
    // These are called by the widget container when this is added or removed. This is the
    // recommended place to acquire rendering resources
    DLLEXPORT virtual void OnAddedToContainer(WidgetContainer* container);
    DLLEXPORT virtual void OnRemovedFromContainer(WidgetContainer* container);

public:
    REFERENCE_COUNTED_PTR_TYPE(Widget);

protected:
    const int ID;
};

}} // namespace Leviathan::GUI
