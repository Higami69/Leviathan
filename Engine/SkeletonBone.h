#ifndef LEVIATHAN_SKELETONBONE
#define LEVIATHAN_SKELETONBONE
// ------------------------------------ //
#ifndef LEVIATHAN_DEFINE
#include "Define.h"
#endif
// ------------------------------------ //
// ---- includes ---- //
#include "BaseObject.h"

namespace Leviathan{ namespace GameObject{

	class SkeletonBone : public BaseObject{
		// friend to skeleton rig for sake of keeping it easy //
		friend class SkeletonRig;

	public:
		DLLEXPORT SkeletonBone::SkeletonBone();
		DLLEXPORT SkeletonBone::SkeletonBone(const wstring &name, const Float3 &position, int group);
		DLLEXPORT SkeletonBone::~SkeletonBone();

		DLLEXPORT void SetName(const wstring &name);
		DLLEXPORT void SetRestPosition(const Float3 &val);
		DLLEXPORT void SetAnimationPosition(const Float3 &val);
		DLLEXPORT void SetBoneGroup(int group);
		DLLEXPORT void SetParentName(const wstring &name);

		DLLEXPORT Float3& GetRestPosition();
		DLLEXPORT void SetPosePosition(const Float3 &pos);

	private:
		wstring Name;
		Float3 RestPosition;
		Float3 AnimationPosition;

		// bone group //
		int BoneGroup;

		weak_ptr<SkeletonBone> Parent;
		wstring ParentName;
	};

}}
#endif