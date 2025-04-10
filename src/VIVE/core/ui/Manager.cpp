#include "Manager.h"
#include "View.h"
#include "Element.h"
#include "Group.h"
#include "Menu.h"
#include "ButtonGroup.h"

#include <cctype>
#include <cassert>
#include <iostream>
#include <algorithm>
/*
#include <osgGA/GUIActionAdapter>
#include <osgGA/GUIEventAdapter>*/

#include <OpenVRUI/coMouseButtonInteraction.h>
#include <OpenVRUI/coTrackerButtonInteraction.h>
#include <OpenVRUI/coRelativeButtonInteraction.h>
#include "vvMSController.h"
#include "vvPluginSupport.h"
#include <net/tokenbuffer.h>
#include <net/message.h>

#include <config/CoviseConfig.h>
#include <util/string_util.h>

namespace vive {
namespace ui {

Manager::Manager()
: Owner("Manager", this)
{
    m_wheelInteraction.push_back(new vrui::coMouseButtonInteraction(vrui::coInteraction::WheelVertical, "MouseWheel", vrui::coInteraction::Low));
    m_wheelInteraction.push_back(new vrui::coMouseButtonInteraction(vrui::coInteraction::WheelHorizontal, "MouseWheel", vrui::coInteraction::Low));

    for (auto &i: m_wheelInteraction)
        vrui::coInteractionManager::the()->registerInteraction(i);

    for (int button = vrui::coInteraction::ButtonA; button <= vrui::coInteraction::LastButton; ++button)
    {
        auto t = static_cast<vrui::coInteraction::InteractionType>(button);
        m_buttonInteraction.push_back(new vrui::coMouseButtonInteraction(t, "MouseButton", vrui::coInteraction::Low));
        m_buttonInteraction.push_back(new vrui::coTrackerButtonInteraction(t, "TrackerButton", vrui::coInteraction::Low));
        m_buttonInteraction.push_back(new vrui::coRelativeButtonInteraction(t, "RelativeButton", vrui::coInteraction::Low));
    }
}

void Manager::init()
{
    for (auto &i: m_buttonInteraction)
        vrui::coInteractionManager::the()->registerInteraction(i);
}

void Manager::add(Element *elem)
{
    m_newElements.push_back(elem);

    elem->m_id = m_numCreated;
    m_elementsById.emplace(elem->elementId(), elem);
    m_elementsByPath.emplace(elem->path(), elem);
    ++m_numCreated;
}

void Manager::remove(Owner *owner)
{
    owner->clearItems();
}

void Manager::remove(Element *elem)
{
    if (elem->m_removed)
        return;

    auto nit = std::find(m_newElements.begin(), m_newElements.end(), elem);
    if (nit != m_newElements.end())
    {
        // an element is removed, before it was ever updated
        elem->m_parent = nullptr;
        m_newElements.erase(nit);
    }

    //std::cerr << "DESTROY: " << elem->path() << std::endl;
    auto it = m_elements.find(elem->m_order);
    if (it != m_elements.end())
    {
        m_elements.erase(it);
    }
    elem->clearItems();

    for (auto v: m_views)
    {
        v.second->removeElement(elem);
    }

    {
        auto it = m_elementsByPath.find(elem->path());
        if (it != m_elementsByPath.end())
        {
            m_elementsByPath.erase(it);
        }
    }
    {
        auto it = m_elementsById.find(elem->elementId());
        if (it != m_elementsById.end())
        {
            m_elementsById.erase(it);
        }
    }

    elem->m_removed = true;
}

Element *Manager::getById(int id) const
{
    auto it = m_elementsById.find(id);
    if (it == m_elementsById.end())
        return nullptr;

    return it->second;
}

Element *Manager::getByPath(const std::string &path) const
{
    auto it = m_elementsByPath.find(path);
    if (it == m_elementsByPath.end())
        return nullptr;

    return it->second;
}


bool Manager::update()
{
    for (auto v: m_views)
    {
        if (v.second->update())
            m_changed = true;
    }

    while (!m_newElements.empty())
    {
        m_changed = true;
        auto elem = m_newElements.front();
        m_newElements.pop_front();
        elem->m_order = m_elemOrder;
        m_elements.emplace(elem->m_order, elem);
        ++m_elemOrder;

        auto path = elem->path();
        auto config = "COVER.UI."+path;
        bool found = false;
        auto shortcuts = covise::coCoviseConfig::getEntry("shortcuts", config, &found);
        if (found)
        {
            //std::cerr << "ui::Manager: configured shortcuts for " << path << ":";
            elem->clearShortcuts();
            auto list = split(shortcuts, ';');
            for (const auto &s: list)
            {
                //std::cerr << " " << s;
                elem->addShortcut(s);
            }
            //std::cerr << std::endl;
        }

        if (elem->parent())
        {
            auto p = elem->parent();
            p->add(elem);
            assert(elem->m_parent = p);
        }
        for (auto v: m_views)
        {
            v.second->elementFactory(elem);
        }
        elem->update();
    }

    if (m_updateAllElements)
    {
        m_changed = true;

        m_updateAllElements = false;
        for (auto elem: m_elements)
            elem.second->update();
    }

    for (auto &inter: m_wheelInteraction)
    {
        if (inter->wasStarted() || inter->isRunning())
        {
            m_changed = true;
            int c = inter->getWheelCount();
            int pressed = c<0 ? vrui::vruiButtons::WHEEL_DOWN : vrui::vruiButtons::WHEEL_UP;
            if (inter->getType() == vrui::coInteraction::WheelHorizontal)
                pressed = c<0 ? vrui::vruiButtons::WHEEL_LEFT : vrui::vruiButtons::WHEEL_RIGHT;
            int count = std::abs(c);
            for (int i=0; i<count; ++i)
                buttonEvent(pressed);
        }
    }

    for (auto inter: m_buttonInteraction)
    {
        if (inter->wasStarted())
        {
            m_changed = true;
            buttonEvent(inter->getType());
        }
    }

    bool ret = m_changed;
    m_changed = false;
    return ret;
}

void Manager::setChanged()
{
    m_changed = true;
}

bool Manager::addView(View *view)
{
    std::string name = view->name();
    auto it = m_views.find(name);
    if (it != m_views.end())
    {
        return false;
    }

    m_views.emplace(name, view);
    view->m_manager = this;

    for (auto elem: m_elements)
    {
        //std::cerr << "Creating by id: " << elem.first << " -> " << elem.second->path() << std::endl;
        view->elementFactory(elem.second);
    }

    m_updateAllElements = true;

    return true;
}

bool Manager::removeView(const View *view)
{
    std::string name = view->name();
    auto it = m_views.find(name);
    if (it != m_views.end())
    {
        if (it->second == view)
        {
            m_views.erase(it);
            return true;
        }
    }
    return false;
}

bool Manager::removeView(const std::string &name)
{
    auto it = m_views.find(name);
    if (it != m_views.end())
    {
        m_views.erase(it);
        return true;
    }
    return false;
}

void Manager::updateText(const Element *elem) const
{
    for (auto v: m_views)
    {
        v.second->updateText(elem);
    }
}

void Manager::updateEnabled(const Element *elem) const
{
    for (auto v: m_views)
    {
        v.second->updateEnabled(elem);
    }
}

void Manager::updateVisible(const Element *elem) const
{
    for (auto v: m_views)
    {
        v.second->updateVisible(elem);
    }
}

void Manager::updateState(const Button *button) const
{
    for (auto v: m_views)
    {
        v.second->updateState(button);
    }
}

void Manager::updateChildren(const Group *group) const
{
    for (auto v: m_views)
    {
        v.second->updateChildren(group);
    }
}

void Manager::updateChildren(const SelectionList *sl) const
{
    for (auto v: m_views)
    {
        v.second->updateChildren(sl);
    }
}

void Manager::updateScale(const Slider *slider) const
{
    for (auto v: m_views)
    {
        v.second->updateScale(slider);
    }
}

void Manager::updateIntegral(const Slider *slider) const
{
    for (auto v: m_views)
    {
        v.second->updateIntegral(slider);
    }
}

void Manager::updateValue(const Slider *slider) const
{
    for (auto v: m_views)
    {
        v.second->updateValue(slider);
    }
}

void Manager::updateBounds(const Slider *slider) const
{
    for (auto v: m_views)
    {
        v.second->updateBounds(slider);
    }
}

void Manager::updateValue(const TextField *input) const
{
    for (auto v: m_views)
    {
        v.second->updateValue(input);
    }
}

void Manager::updateFilter(const FileBrowser *fb) const
{
    for (auto v: m_views)
    {
        v.second->updateFilter(fb);
    }
}

void Manager::updateViewpoint(const CollaborativePartner *cp) const
{
    for (auto v: m_views)
    {
        v.second->updateViewpoint(cp);
    }
}

void Manager::updateRelayout(const Group* gr) const
{
    for (auto v : m_views)
    {
        v.second->updateRelayout(gr);
    }
}

bool Manager::keyEvent(vsg::KeyPressEvent& keyPress)
{
    std::string handled;
    m_modifiers = (int)keyPress.keyModifier;


        bool alt = keyPress.keyModifier & vsg::MODKEY_Alt;
        bool ctrl = keyPress.keyModifier & vsg::MODKEY_Control;
        bool shift = keyPress.keyModifier & vsg::MODKEY_Shift;
        bool meta = keyPress.keyModifier & vsg::MODKEY_Meta;
        int keySym = keyPress.keyBase;

        if (shift && keySym <= 255 && std::isupper(keySym))
        {
            //std::cerr << "ui::Manager: mapping to lower" << std::endl;
            keySym = std::tolower(keySym);
        }

        for (auto& elemPair : m_elements)
        {
            auto& elem = elemPair.second;
            if (elem->enabled() && elem->matchShortcut(m_modifiers, keySym))
            {
                elem->shortcutTriggered();
                if (!handled.empty())
                {
                    std::cerr << "ui::Manager: duplicate mapping for keyboard shortcut on " << elem->path() << " and " << handled << std::endl;
                }
                handled = elem->path();
                continue;
            }
        }

        /*
    if (type == osgGA::GUIEventAdapter::RELEASE
             || type == osgGA::GUIEventAdapter::SCROLL)
    {
        std::cerr << "mouse: ";

        int button = 0;
        int modifiers = 0;
        if (type == osgGA::GUIEventAdapter::RELEASE)
        {
            if (mod == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
                button = Left;
            if (mod == osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON)
                button = Middle;
            if (mod == osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
                button = Right;
        }
        else
        {
            if (mod == osgGA::GUIEventAdapter::SCROLL_UP)
                button = ScrollUp;
            if (mod == osgGA::GUIEventAdapter::SCROLL_DOWN)
                button = ScrollDown;
            if (mod == osgGA::GUIEventAdapter::SCROLL_LEFT)
                button = ScrollLeft;
            if (mod == osgGA::GUIEventAdapter::SCROLL_RIGHT)
                button = ScrollRight;
        }

        switch (button)
        {
        case Left:
            std::cerr << "Left" << std::endl;
            break;
        case Middle:
            std::cerr << "Middle" << std::endl;
            break;
        case Right:
            std::cerr << "Right" << std::endl;
            break;
        case ScrollUp:
            std::cerr << "ScrollUp" << std::endl;
            break;
        case ScrollDown:
            std::cerr << "ScrollDown" << std::endl;
            break;
        }

        if (button != 0)
        {
            for (auto &elemPair: m_elements)
            {
                auto &elem = elemPair.second;
                if (elem->enabled() && elem->matchButton(modifiers, button))
                {
                    elem->shortcutTriggered();
                    if (!handled.empty())
                    {
                        std::cerr << "ui::Manager: duplicate mapping for mouse button on " << elem->path() << " and " << handled << std::endl;
                    }
                    handled = elem->path();
                    continue;
                }
            }
        }
    }*/

    return !handled.empty();
}

bool Manager::buttonEvent(int button) const
{
    std::string handled;

    //std::cerr << "ui::Manager::buttonEvent: button=0x" << std::hex << button << ", modifiers=" << m_modifiers << std::dec << std::endl;
    for (auto &elemPair: m_elements)
    {
        auto &elem = elemPair.second;
        if (elem->enabled() && elem->matchButton(m_modifiers, button))
        {
            elem->shortcutTriggered();
            if (!handled.empty())
            {
                std::cerr << "ui::Manager: duplicate mapping for button on " << elem->path() << " and " << handled << std::endl;
            }
            handled = elem->path();
            continue;
        }
    }

    return !handled.empty();
}

void Manager::flushUpdates()
{

    for (const auto &state: m_elemState)
    {
        auto id = state.first;
        auto mask = state.second.first;

        m_updates << id;
        m_updates << mask;
        m_updates << false; // trigger
        m_updates << state.second.second;

        ++m_numUpdates;
    }
    m_elemState.clear();
}

void Manager::queueUpdate(const Element *elem, Element::UpdateMaskType mask, bool trigger)
{
    if (elem->elementId() < 0)
    {
        assert(!trigger);
        return;
    }

    assert(elem->elementId() >= 0);

    auto it = m_elemState.find(elem->elementId());
    if (it != m_elemState.end())
        mask |= it->second.first;
    if (trigger)
    {
        if (it != m_elemState.end())
            m_elemState.erase(it);
        flushUpdates();

        covise::TokenBuffer tb;
        elem->save(tb);

        m_updates << elem->elementId();
        m_updates << mask;
        m_updates << trigger;
        m_updates << tb;

        ++m_numUpdates;
    }
    else
    {
        if (it == m_elemState.end())
        {
            it = m_elemState.emplace(elem->elementId(), std::make_pair(Element::UpdateMaskType(0), covise::TokenBuffer{})).first;
        }
        else
        {
            it->second.second = covise::TokenBuffer();
        }
        it->second.first = mask;
        elem->save(it->second.second);
    }
}

void Manager::processUpdates(covise::TokenBuffer &updates, int numUpdates, bool runTriggers, std::set<ButtonGroup *> &delayed)
{

    for (int i=0; i<numUpdates; ++i)
    {
        //std::cerr << "processing " << i << std::flush;
        int id = -1;
        updates >> id;
        Element::UpdateMaskType mask(0);
        updates >> mask;
        bool trigger = false;
        updates >> trigger;
        covise::TokenBuffer tb;
        updates >> tb;
        auto elem = getById(id);
        if (!elem)
        {
            if (vv->debugLevel(2))
                std::cerr << "ui::Manager::processUpdates NOT FOUND: id=" << id << ", trigger=" << trigger << std::endl;
            continue;
        }
        if (vv->debugLevel(5))
            std::cerr << "ui::Manager::processUpdates for id=" << id << ": " << elem->path() << std::endl;
        //std::cerr << ": id=" << id << ", trigger=" << trigger << std::endl;
        assert(elem);
        elem->load(tb);
        elem->update(mask);
        if (trigger)
        {
            if (auto bg = dynamic_cast<ButtonGroup *>(elem))
            {
                delayed.insert(bg);
            }
            else if (runTriggers)
            {
                setChanged();
                elem->triggerImplementation();
            }
            else
            {
                std::cerr << "ui::Manager::processUpdates: path=" << elem->path() << " still would trigger" << std::endl;
            }
        }
    }
}

bool Manager::sync()
{
    bool changed = false;

    flushUpdates();
    auto ms = vvMSController::instance();
    if (ms->isCluster())
    {
        ms->syncData(&m_numUpdates, sizeof(m_numUpdates));
        if (m_numUpdates > 0)
        {
            //std::cerr << "ui::Manager: syncing " << m_numUpdates << " updates" << std::endl;
            if (ms->isMaster())
            {
                covise::Message msg(m_updates);
                vvMSController::instance()->syncMessage(&msg);
            }
            else
            {
                covise::Message msg;
                vvMSController::instance()->syncMessage(&msg);
                m_updates = covise::TokenBuffer{ &msg };
            }
        }
    }


    std::set<ButtonGroup *> delayed;
    int round = 0;
    while (m_numUpdates > 0)
    {
        changed = true;

        //std::cerr << "ui::Manager: processing " << m_numUpdates << " updates in round " << round << std::endl;

        covise::TokenBuffer updates{ m_updates };
        updates.rewind();

        m_updates = covise::TokenBuffer{};
        int numUpdates = m_numUpdates;
        m_numUpdates = 0;


        if (round > 2)
            break;

        processUpdates(updates, numUpdates, round<1, delayed);
        ++round;
    }

    for (auto &bg: delayed)
    {
        setChanged();
        bg->triggerImplementation();
    }

    //assert(m_numUpdates == 0);
    return changed;
}

std::vector<const Element *> Manager::getAllElements() const
{
    std::vector<const Element *> result;
    for (auto e: m_elements)
    {
        result.push_back(e.second);
    }
    return result;
}

}
}
