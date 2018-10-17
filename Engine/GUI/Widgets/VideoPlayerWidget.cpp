// ------------------------------------ //
#include "VideoPlayerWidget.h"

#include "GUI/GuiWidgetContainer.h"
#include "Rendering/GeometryHelpers.h"
#include "Window.h"

#include "OgreItem.h"
#include "OgreMaterialManager.h"
#include "OgreMeshManager2.h"
#include "OgreSceneManager.h"
#include "OgreSubMesh2.h"
#include "OgreTechnique.h"

using namespace Leviathan;
using namespace Leviathan::GUI;
// ------------------------------------ //
DLLEXPORT VideoPlayerWidget::VideoPlayerWidget()
{
    Player.OnPlayBackEnded.Register(LambdaDelegateSlot::MakeShared<LambdaDelegateSlot>(
        [=](const NamedVars::pointer& values) {
            if(Callback)
                Callback();
        }));
}

DLLEXPORT VideoPlayerWidget::~VideoPlayerWidget()
{
    Callback = nullptr;
}
// ------------------------------------ //
DLLEXPORT bool VideoPlayerWidget::Play(const std::string& videofile)
{
    if(!Player.Play(videofile))
        return false;

    // The Play method creates the texture we want to display

    // Set the texture on our cloned material
    auto* pass = Material->getTechnique(0)->getPass(0);

    Ogre::TextureUnitState* textureState;

    if(pass->getNumTextureUnitStates() < 1) {
        textureState = Material->getTechnique(0)->getPass(0)->createTextureUnitState();
    } else {
        textureState = pass->getTextureUnitState(0);
    }

    textureState->setTexture(Player.GetTexture());
    textureState->setHardwareGammaEnabled(true);
    Material->compile();

    QuadMesh->getSubMesh(0)->setMaterialName(Material->getName());


    // Recreate item
    Ogre::SceneManager* scene = ContainedIn->GetWindow()->GetOverlayScene();

    if(QuadItem) {
        scene->destroyItem(QuadItem);
        QuadItem = nullptr;
    }

    QuadItem = scene->createItem(QuadMesh, Ogre::SCENE_STATIC);
    QuadItem->setCastShadows(false);

    QuadItem->setRenderQueueGroup(2);

    // Add it
    Node->attachObject(QuadItem);

    return true;
}
// ------------------------------------ //
DLLEXPORT void VideoPlayerWidget::SetEndCallback(std::function<void()> callback)
{
    Callback = callback;
}
// ------------------------------------ //
DLLEXPORT void VideoPlayerWidget::OnAddedToContainer(WidgetContainer* container)
{
    ContainedIn = container;

    QuadMesh = GeometryHelpers::CreateScreenSpaceQuad(
        "videoplayer_widget_" + std::to_string(ID) + "_mesh", -1, -1, 2, 2);

    // Duplicate the material
    Ogre::MaterialPtr baseMaterial =
        Ogre::MaterialManager::getSingleton().getByName("GUIOverlay");

    if(!baseMaterial)
        LOG_FATAL(
            "VideoPlayerWidget: GUIOverlay material doesn't exists! are the core Leviathan "
            "materials and shaders copied?");

    Material = baseMaterial->clone("videoplayer_widget_" + std::to_string(ID) + "_material");

    Ogre::SceneManager* scene = ContainedIn->GetWindow()->GetOverlayScene();

    Node = scene->createSceneNode(Ogre::SCENE_STATIC);

    // Setup render queue for it
    // TODO: a proper system needs to be done for managing what is on top of what
    scene->getRenderQueue()->setRenderQueueMode(2, Ogre::RenderQueue::FAST);
}

DLLEXPORT void VideoPlayerWidget::OnRemovedFromContainer(WidgetContainer* container)
{
    if(!ContainedIn)
        return;

    Ogre::SceneManager* scene = ContainedIn->GetWindow()->GetOverlayScene();

    if(Node) {
        scene->destroySceneNode(Node);
        Node = nullptr;
    }

    if(QuadItem) {
        scene->destroyItem(QuadItem);
        QuadItem = nullptr;
    }

    if(QuadMesh) {
        Ogre::MeshManager::getSingleton().remove(QuadMesh);
        QuadMesh.reset();
    }

    if(Material) {
        Ogre::MaterialManager::getSingleton().remove(Material);
        Material.reset();
    }
}