#include <stdio.h>
#include <iostream>

#include <Common/Base/keycode.cxx> 
#include <Common/Base/Config/hkProductFeatures.cxx>

#include <Common/Base/Memory/System/hkMemorySystem.h>
#include <Common/Base/Memory/System/Util/hkMemoryInitUtil.h>
#include <Common/Base/Memory/Allocator/Malloc/hkMallocAllocator.h>

#include <Common/Base/System/Io/IStream/hkIStream.h>
#include <Common/Base/System/Io/Reader/hkStreamReader.h>
#include <Common/Base/System/Io/OStream/hkOStream.h>
#include <Common/Base/System/Io/Writer/hkStreamWriter.h>

//#include <Common/Base/Reflection/Registry/hkDynamicClassNameRegistry.h>
#include <Common/Base/Reflection/Registry/hkDefaultClassNameRegistry.h>

// Serialize Loader
#include <Common/Serialize/Util/hkSerializeUtil.h>
#include <Common/Serialize/Util/hkRootLevelContainer.h>
#include <Common/Serialize/Util/hkNativePackfileUtils.h>
#include <Common/Serialize/Util/hkLoader.h>
#include <Common/Serialize/Version/hkVersionPatchManager.h>
#include <Common/Compat/Deprecated/Packfile/Binary/hkBinaryPackfileReader.h>
#include <Common/Compat/Deprecated/Packfile/Xml/hkXmlPackfileReader.h>

// Animation
#include <Animation/Animation/hkaAnimationContainer.h>
#include <Animation/Animation/Rig/hkaPose.h>
#include <Animation/Animation/Rig/hkaSkeleton.h>
#include <Animation/Animation/Rig/hkaSkeletonUtils.h>

#include <Animation/Animation/Animation/SplineCompressed/hkaSplineCompressedAnimation.h>

#pragma comment (lib, "hkBase.lib")
#pragma comment (lib, "hkSerialize.lib")
#pragma comment (lib, "hkSceneData.lib")
#pragma comment (lib, "hkInternal.lib")
#pragma comment (lib, "hkGeometryUtilities.lib")
//o1: #pragma comment (lib, "hkVisualize.lib")
#pragma comment (lib, "hkCompat.lib")
#pragma comment (lib, "hkpCollide.lib")
#pragma comment (lib, "hkpConstraintSolver.lib")
#pragma comment (lib, "hkpDynamics.lib")
#pragma comment (lib, "hkpInternal.lib")
#pragma comment (lib, "hkpUtilities.lib")
#pragma comment (lib, "hkpVehicle.lib")
#pragma comment (lib, "hkaAnimation.lib")
#pragma comment (lib, "hkaRagdoll.lib")
#pragma comment (lib, "hkaInternal.lib")
//o1: #pragma comment (lib, "hkgBridge.lib")

void HK_CALL errorReport(const char* msg, void* userContext)
{
	std::cerr << msg << std::endl;
}

hkResult hkSerializeUtilSave( hkVariant& root, hkOstream& stream )
{
	hkPackfileWriter::Options packFileOptions;
	packFileOptions.m_layout = hkStructureLayout::MsvcWin32LayoutRules;

	hkResult res;
	__try
	{
		res = hkSerializeUtil::savePackfile( root.m_object, *root.m_class, stream.getStreamWriter(), packFileOptions );
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		res = HK_FAILURE;
	}
	if ( res != HK_SUCCESS )
	{
		std::cerr << "Havok reports save failed.";
	}
	return res;
}

void read(hkIstream& stream, hkQsTransform& transform)
{
	hkVector4 translation;
	hkQuaternion rotation;
	hkVector4 scale;

	stream.read(&translation, sizeof(hkVector4));
	stream.read(&rotation.m_vec, sizeof(hkVector4));
	stream.read(&scale, sizeof(hkVector4));

	transform.setTranslation(translation);
	transform.setRotation(rotation);
	transform.setScale(scale);
}

void read(hkIstream& stream, hkaSkeleton *skeleton)
{
	{
		char buf[240];
		int len = stream.getline(buf, 240, '\0');
		//printf("len %d\n", len);
		puts(buf);

		skeleton->m_name = buf;
	}

	int nparentIndices;
	stream.read(&nparentIndices, sizeof(int));
	printf("#parentIndices: %d\n", nparentIndices);
	skeleton->m_parentIndices.setSize(nparentIndices);

	for (int i=0; i<nparentIndices; ++i)
	{
		stream.read(&skeleton->m_parentIndices[i], sizeof(hkInt16));
	}

	int nbones;
	stream.read(&nbones, sizeof(int));
	printf("#nbones: %d\n", nbones);
	skeleton->m_bones.setSize(nbones);

	for (int i=0; i<nbones; ++i)
	{
		char buf[240];
		int len = stream.getline(buf, 240, '\0');
		//printf("len %d\n", len);
		puts(buf);

		skeleton->m_bones[i].m_name = buf;
	}

	int nreferencePose;
	stream.read(&nreferencePose, sizeof(int));
	printf("#nreferencePose: %d\n", nreferencePose);
	skeleton->m_referencePose.setSize(nreferencePose);

	for (int i=0; i<nreferencePose; ++i)
	{
		read(stream, skeleton->m_referencePose[i]);
	}

	int nreferenceFloats;
	stream.read(&nreferenceFloats, sizeof(int));
	printf("#nreferenceFloats: %d\n", nreferenceFloats);
	skeleton->m_referenceFloats.setSize(nreferenceFloats);

	for (int i=0; i<nreferenceFloats; ++i)
	{
		hkReal g;

		stream.read(&g, sizeof(hkReal));

		skeleton->m_referenceFloats[i] = g;
	}

	int nfloatSlots;
	stream.read(&nfloatSlots, sizeof(int));
	printf("#nfloatSlots: %d\n", nfloatSlots);
	skeleton->m_floatSlots.setSize(nfloatSlots);

	for (int i=0; i<nfloatSlots; ++i)
	{
		char buf[240];
		int len = stream.getline(buf, 240, '\0');
		//printf("len %d\n", len);
		puts(buf);

		skeleton->m_floatSlots[i] = buf;
	}
}

void read(hkIstream& stream, hkaInterleavedUncompressedAnimation *anim)
{
	int numOriginalFrames;
	hkReal duration;
	int numTransforms;
	int numFloats;

	stream.read(&numOriginalFrames, sizeof(int));
	stream.read(&duration, sizeof(hkReal));
	stream.read(&numTransforms, sizeof(int));
	stream.read(&numFloats, sizeof(int));

	printf("numOriginalFrames: %d\n", numOriginalFrames);
	printf("duration: %.6f\n", duration);
	printf("numTransforms: %d\n", numTransforms);
	printf("numFloats: %d\n", numFloats);

	anim->m_duration = duration;
	anim->m_numberOfTransformTracks = numTransforms;
	anim->m_numberOfFloatTracks = numFloats;
	anim->m_transforms.setSize(numTransforms * numOriginalFrames, hkQsTransform::getIdentity());
	anim->m_floats.setSize(numFloats * numOriginalFrames);

	for (int f=0; f<numOriginalFrames; f++)
	{
		hkReal time;
		stream.read(&time, sizeof(hkReal));
		printf("time: %.6f\n", time);

		int offTransforms = f * numTransforms;

		for (int i=0; i<numTransforms; i++)
		{
			read(stream, anim->m_transforms[i + offTransforms]);
		}

		int offFloats = f * numFloats;

		for (int i=0; i<numFloats; i++)
		{
			hkReal g;

			stream.read(&g, sizeof(hkReal));

			anim->m_floats[i + offFloats] = g;
		}
	}
}

int save(const char* filename, const char* destname)
{
	hkIstream stream(filename);

	char buf[240];
	int len = stream.getline(buf, 240, '\n');
	//printf("len %d\n", len);
	puts(buf);

	unsigned int version;
	stream.read(&version, sizeof(unsigned int));

	if (version != 0x01000000)
	{
		std::cerr << "Error: version mismatch! Abort.";
		return 100;
	}

	int nskeletons;
	stream.read(&nskeletons, sizeof(int));

	int nanimations;
	stream.read(&nanimations, sizeof(int));

	if (nskeletons != 0)
	{
		// TEST
		hkRefPtr<hkaSkeleton> skeleton = new hkaSkeleton();
		read(stream, skeleton);

		std::cerr << "Error: #skeletons should be 0 but " << nskeletons << "! Abort.";
		return 101;
	}

	if (nanimations != 1)
	{
		std::cerr << "Error: #animations should be 1 but " << nanimations << "! Abort.";
		return 101;
	}

	hkRootLevelContainer rootCont;

	hkRefPtr<hkaAnimationContainer> animCont = new hkaAnimationContainer();
	rootCont.m_namedVariants.pushBack( hkRootLevelContainer::NamedVariant("Merged Animation Container", animCont.val(), &animCont->staticClass()) );

	hkRefPtr<hkaAnimationBinding> binding = new hkaAnimationBinding();
	animCont->m_bindings.append(&binding, 1);

	binding->m_originalSkeletonName = "NPC Root [Root]";

	hkRefPtr<hkaInterleavedUncompressedAnimation> anim = new hkaInterleavedUncompressedAnimation();
	read(stream, anim);

	/*
	 * spline compressed
	 */
	hkaSplineCompressedAnimation::TrackCompressionParams tparams;
	hkaSplineCompressedAnimation::AnimationCompressionParams aparams;

	tparams.m_rotationTolerance = 0.001f;
	tparams.m_rotationQuantizationType = hkaSplineCompressedAnimation::TrackCompressionParams::THREECOMP40;

	binding->m_animation = new hkaSplineCompressedAnimation( *anim.val(), tparams, aparams ); 


	animCont->m_animations.append(&binding->m_animation, 1);

	hkVariant root = { &rootCont, &rootCont.staticClass() };
	hkOstream deststream(destname);
	hkSerializeUtilSave(root, deststream);

	return 0;
}

int main( int argc, char *argv[], char *envp[] )
{
	char *ofile = NULL;
	char *ifile = NULL;

	for (int i=1; i<argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
				case '?':
					{
						break;
					}
				case 'o':
					{
						if (++i<argc)
						{
							ofile = argv[i];
						}
						break;
					}
			}
		}
		else
		{
			ifile = argv[i];
		}
	}

	if (ifile == NULL)
	{
		puts("Usage: hkconv.exe -o out.hkx src.bin");
		return 1;
	}

	if (ofile == NULL)
		ofile = "out.hkx";

	// Need to have memory allocated for the solver. Allocate 1mb for it.
	hkMemoryRouter *memoryRouter = hkMemoryInitUtil::initDefault( hkMallocAllocator::m_defaultMallocAllocator, hkMemorySystem::FrameInfo(1024 * 1024) );
	hkBaseSystem::init( memoryRouter, errorReport );

	int ret = save(ifile, ofile);

	// Shutdown
	hkBaseSystem::quit();
	hkMemoryInitUtil::quit();

	return ret;
}
