#include <stdio.h>
#include <iostream>

#include <Common/Base/keycode.cxx> 
#include <Common/Base/Config/hkProductFeatures.cxx>

#include <Common/Base/hkBase.h>
#include <Common/Base/System/hkBaseSystem.h>
#include <Common/Base/System/Error/hkDefaultError.h>
#include <Common/Base/Memory/System/hkMemorySystem.h>
#include <Common/Base/Memory/System/Util/hkMemoryInitUtil.h>
#include <Common/Base/Memory/Allocator/Malloc/hkMallocAllocator.h>

#include <Common/Base/System/Io/IStream/hkIStream.h>
#include <Common/Base/System/Io/Reader/hkStreamReader.h>
#include <Common/Base/System/Io/OStream/hkOStream.h>
#include <Common/Base/System/Io/Writer/hkStreamWriter.h>

#include <Common/Base/Reflection/Registry/hkDynamicClassNameRegistry.h>
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

#pragma comment (lib, "hkBase.lib")
#pragma comment (lib, "hkSerialize.lib")
#pragma comment (lib, "hkSceneData.lib")
#pragma comment (lib, "hkInternal.lib")
#pragma comment (lib, "hkGeometryUtilities.lib")
#pragma comment (lib, "hkVisualize.lib")
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
#pragma comment (lib, "hkgBridge.lib")

void HK_CALL errorReport(const char* msg, void* userContext)
{
	std::cerr << msg << std::endl;
}

hkResource* hkSerializeUtilLoad( hkStreamReader* stream
		, hkSerializeUtil::ErrorDetails* detailsOut/*=HK_NULL*/
		, const hkClassNameRegistry* classReg/*=HK_NULL*/
		, hkSerializeUtil::LoadOptions options/*=hkSerializeUtil::LOAD_DEFAULT*/ )
{
   __try
   {
	  return hkSerializeUtil::load(stream, detailsOut, classReg, options);
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
	  if (detailsOut == NULL)
		 detailsOut->id = hkSerializeUtil::ErrorDetails::ERRORID_LOAD_FAILED;
	  return NULL;
   }
}

hkResult hkSerializeLoad(hkStreamReader *reader
		, hkVariant &root
		, hkResource *&resource)
{
   hkTypeInfoRegistry &defaultTypeRegistry = hkTypeInfoRegistry::getInstance();
   hkDefaultClassNameRegistry &defaultRegistry = hkDefaultClassNameRegistry::getInstance();

   hkBinaryPackfileReader bpkreader;
   hkXmlPackfileReader xpkreader;
   resource = NULL;
   hkSerializeUtil::FormatDetails formatDetails;
   hkSerializeUtil::detectFormat( reader, formatDetails );
   hkBool32 isLoadable = hkSerializeUtil::isLoadable( reader );
   if (!isLoadable && formatDetails.m_formatType != hkSerializeUtil::FORMAT_TAGFILE_XML)
   {
	  return HK_FAILURE;
   }
   else
   {
	  switch ( formatDetails.m_formatType )
	  {
	  case hkSerializeUtil::FORMAT_PACKFILE_BINARY:
		 {
			bpkreader.loadEntireFile(reader);
			bpkreader.finishLoadedObjects(defaultTypeRegistry);
			if ( hkPackfileData* pkdata = bpkreader.getPackfileData() )
			{
			   hkArray<hkVariant>& obj = bpkreader.getLoadedObjects();
			   for ( int i =0,n=obj.getSize(); i<n; ++i)
			   {
				  hkVariant& value = obj[i];
				  if ( value.m_class->hasVtable() )
					 defaultTypeRegistry.finishLoadedObject(value.m_object, value.m_class->getName());
			   }
			   resource = pkdata;
			   resource->addReference();
			}
			root = bpkreader.getTopLevelObject();
		 }
		 break;

	  case hkSerializeUtil::FORMAT_PACKFILE_XML:
		 {
			xpkreader.loadEntireFileWithRegistry(reader, &defaultRegistry);
			if ( hkPackfileData* pkdata = xpkreader.getPackfileData() )
			{
			   hkArray<hkVariant>& obj = xpkreader.getLoadedObjects();
			   for ( int i =0,n=obj.getSize(); i<n; ++i)
			   {
				  hkVariant& value = obj[i];
				  if ( value.m_class->hasVtable() )
					 defaultTypeRegistry.finishLoadedObject(value.m_object, value.m_class->getName());
			   }
			   resource = pkdata;
			   resource->addReference();
			   root = xpkreader.getTopLevelObject();
			}
		 }
		 break;

	  case hkSerializeUtil::FORMAT_TAGFILE_BINARY:
	  case hkSerializeUtil::FORMAT_TAGFILE_XML:
	  default:
		 {
			hkSerializeUtil::ErrorDetails detailsOut;
			hkSerializeUtil::LoadOptions loadflags = hkSerializeUtil::LOAD_FAIL_IF_VERSIONING;
			resource = hkSerializeUtilLoad(reader, &detailsOut, &defaultRegistry, loadflags);
			root.m_object = resource->getContents<hkRootLevelContainer>();
			if (root.m_object != NULL)
			   root.m_class = &((hkRootLevelContainer*)root.m_object)->staticClass();
		 }
		 break;
	  }
   }
   return root.m_object != NULL ? HK_SUCCESS : HK_FAILURE;
}

hkResource *hkSerializeLoadResource(hkStreamReader *reader)
{
   hkResource *resource = NULL;
   hkVariant root;   
   hkSerializeLoad(reader, root, resource);
   return resource;
}

void write(hkOstream& o, hkQsTransform& transform)
{
	const hkVector4& translation = transform.getTranslation();
	const hkQuaternion& rotation = transform.getRotation();
	const hkVector4& scale = transform.getScale();

	float tx = translation(0);
	float ty = translation(1);
	float tz = translation(2);
	float tw = translation(3);

	float rx = rotation.m_vec(0);
	float ry = rotation.m_vec(1);
	float rz = rotation.m_vec(2);
	float rw = rotation.m_vec(3);

	float sx = scale(0);
	float sy = scale(1);
	float sz = scale(2);
	float sw = scale(3);

	o.printf(" %.6f %.6f %.6f", tx, ty, tz);
	o.printf(" %.6f %.6f %.6f %.6f", rx, ry, rz, rw);
	o.printf(" %.6f", sz);
	o.printf("\n");
}

void write(hkOstream& o, hkaSkeleton *skeleton)
{
		/// A user name to aid in identifying the skeleton
	o << skeleton->m_name << "\n";

		/// Parent relationship
	int nparentIndices = skeleton->m_parentIndices.getSize();
	o.printf("#parentIndices: %d\n", nparentIndices);

	for (int i=0; i<nparentIndices; ++i)
	{
		o.printf("%d %d\n", i, skeleton->m_parentIndices[i]);
	}

		/// Bones for this skeleton
	int nbones = skeleton->m_bones.getSize();
	o.printf("#bones: %d\n", nbones);

	for (int i=0; i<nbones; ++i)
	{
		o << skeleton->m_bones[i].m_name << "\n";
	}

		/// The reference pose for the bones of this skeleton. This pose is stored in local space.
	int nreferencePose = skeleton->m_referencePose.getSize();
	o.printf("#referencePose: %d\n", nreferencePose);

	for (int i=0; i<nreferencePose; ++i)
	{
		o.printf("%d", i);
		write(o, skeleton->m_referencePose[i]);
	}

		/// The reference values for the float slots of this skeleton. This pose is stored in local space.
	int nreferenceFloats = skeleton->m_referenceFloats.getSize();
	o.printf("#referenceFloats: %d\n", nreferenceFloats);

	for (int i=0; i<nreferenceFloats; ++i)
	{
		o.printf("%d %.6f\n", i, skeleton->m_referenceFloats[i]);
	}

		/// Floating point track slots. Often used for auxiliary float data or morph target parameters etc.
		/// This defines the target when binding animations to a particular rig.
	int nfloatSlots = skeleton->m_floatSlots.getSize();
	o.printf("#floatSlots: %d\n", nfloatSlots);

	for (int i=0; i<nfloatSlots; ++i)
	{
		o << skeleton->m_floatSlots[i] << "\n";
	}
}

void write(hkOstream& o, hkaAnimation *anim)
{
		/// Returns the number of original samples / frames of animation.
	int numOriginalFrames = anim->getNumOriginalFrames();
		/// The length of the animation cycle in seconds
	hkReal duration = anim->m_duration;
		/// The number of bone tracks to be animated.
	int numTransforms = anim->m_numberOfTransformTracks;
		/// The number of float tracks to be animated
	int numFloats = anim->m_numberOfFloatTracks;

	o.printf("numOriginalFrames: %d\n", numOriginalFrames);
	o.printf("duration: %.6f\n", duration);
	o.printf("numberOfTransformTracks: %d\n", numTransforms);
	o.printf("numberOfFloatTracks: %d\n", numFloats);

	hkLocalArray<hkQsTransform> transformOut(numTransforms);
	hkLocalArray<hkReal> floatsOut(numFloats);

	transformOut.setSize(numTransforms);
	floatsOut.setSize(numFloats);

	hkReal time = 0.0f;

		/// Get a subset of the first 'maxNumTracks' transform tracks (all tracks from 0 to maxNumTracks-1 inclusive), and the first 'maxNumFloatTracks' float tracks of a pose at a given time.
	anim->samplePartialTracks(time, numTransforms, transformOut.begin(), numFloats, floatsOut.begin(), HK_NULL);
	hkaSkeletonUtils::normalizeRotations(transformOut.begin(), numTransforms);

	o.printf("time: %.6f\n", time);

	for (int i=0; i<numTransforms; ++i)
	{
		o.printf("%d", i);
		write(o, transformOut[i]);
	}

	for (int i=0; i<numFloats; ++i)
	{
		o.printf("%d %.6f\n", floatsOut[i]);
	}
}

void write(hkOstream& o, hkaAnimationBinding *binding)
{
		/// (Optional) The name of the skeleton from which the binding was constructed.
	o << binding->m_originalSkeletonName << "\n";

		/// A handle to the animation bound to the skeleton.
	o << "animation: todo\n";

		/// A mapping from animation track indices to bone indices.
	int ntransformToBoneIndices = binding->m_transformTrackToBoneIndices.getSize();
	o.printf("#transformToBoneIndices: %d\n", ntransformToBoneIndices);

	for (int i=0; i<ntransformToBoneIndices; i++)
	{
		o.printf("%d %d\n", i, binding->m_transformTrackToBoneIndices[i]);
	}

		/// A mapping from animation float track indices to float slot indices.
	int nfloatTrackToFloatSlotIndices = binding->m_floatTrackToFloatSlotIndices.getSize();
	o.printf("#floatTrackToFloatSlotIndices: %d\n", nfloatTrackToFloatSlotIndices);

	for (int i=0; i<nfloatTrackToFloatSlotIndices; i++)
	{
		o.printf("%d %d\n", i, binding->m_floatTrackToFloatSlotIndices[i]);
	}
}

void dump(const char* filename, const char* destname)
{
	hkOstream o(destname);

	o << "hkdump File Format, Version 1.0.0.0\n";

	hkIstream stream(filename);
	hkStreamReader *reader = stream.getStreamReader();
	hkResource *resource = hkSerializeLoadResource(reader);
	if (resource == NULL)
	{
		std::cerr << "File is not loadable" << std::endl;
	}
	else
	{
		if (hkRootLevelContainer* scene = resource->getContents<hkRootLevelContainer>())
		{
			if (hkaAnimationContainer *animCont = scene->findObject<hkaAnimationContainer>())
			{
				int nskeletons = animCont->m_skeletons.getSize();
				o.printf("#skeletons: %d\n", nskeletons);

				if (nskeletons != 0)
				{
					hkaSkeleton *skeleton = animCont->m_skeletons[0];
					write(o, skeleton);
				}

				int nanimations = animCont->m_animations.getSize();
				o.printf("#animations: %d\n", nanimations);

				if (nanimations != 0)
				{
					hkaAnimation *animation = animCont->m_animations[0];
					write(o, animation);
				}

				/*
				int nbindings = animCont->m_bindings.getSize();
				o.printf("#bindings: %d\n", nbindings);

				if (nbindings != 0)
				{
					hkaAnimationBinding *binding = animCont->m_bindings[0];
					write(o, binding);
				}
				*/
			}
		}
		resource->removeReference();
	}
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
		puts("Usage: hkdump.exe -o out.txt src.hkx");
		return 1;
	}

	if (ofile == NULL)
		ofile = "out.txt";

	// Need to have memory allocated for the solver. Allocate 1mb for it.
	hkMemoryRouter *memoryRouter = hkMemoryInitUtil::initDefault( hkMallocAllocator::m_defaultMallocAllocator, hkMemorySystem::FrameInfo(1024 * 1024) );
	hkBaseSystem::init( memoryRouter, errorReport );

	dump(ifile, ofile);

	// Shutdown
	hkBaseSystem::quit();
	hkMemoryInitUtil::quit();

	return 0;
}
