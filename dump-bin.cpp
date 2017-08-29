#include <stdio.h>
#include <iostream>

#include <shlwapi.h>

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

#pragma comment(lib, "shlwapi.lib")

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

void write(hkStreamWriter *writer, hkQsTransform& transform)
{
	const hkVector4& translation = transform.getTranslation();
	const hkQuaternion& rotation = transform.getRotation();
	const hkVector4& scale = transform.getScale();

	writer->write(&translation, sizeof(hkVector4));
	writer->write(&rotation.m_vec, sizeof(hkVector4));
	writer->write(&scale, sizeof(hkVector4));
}

void write(hkStreamWriter *writer, hkaSkeleton *skeleton)
{
		/// A user name to aid in identifying the skeleton
	hkStringPtr name = skeleton->m_name;
	writer->write(name.cString(), name.getLength()+1);

		/// Parent relationship
	int nparentIndices = skeleton->m_parentIndices.getSize();
	writer->write(&nparentIndices, sizeof(int));

	for (int i=0; i<nparentIndices; ++i)
	{
		writer->write(&skeleton->m_parentIndices[i], sizeof(hkInt16));
	}

		/// Bones for this skeleton
	int nbones = skeleton->m_bones.getSize();
	writer->write(&nbones, sizeof(int));

	for (int i=0; i<nbones; ++i)
	{
		hkStringPtr name = skeleton->m_bones[i].m_name;
		writer->write(name.cString(), name.getLength()+1);
	}

		/// The reference pose for the bones of this skeleton. This pose is stored in local space.
	int nreferencePose = skeleton->m_referencePose.getSize();
	writer->write(&nreferencePose, sizeof(int));

	for (int i=0; i<nreferencePose; ++i)
	{
		write(writer, skeleton->m_referencePose[i]);
	}

		/// The reference values for the float slots of this skeleton. This pose is stored in local space.
	int nreferenceFloats = skeleton->m_referenceFloats.getSize();
	writer->write(&nreferenceFloats, sizeof(int));

	for (int i=0; i<nreferenceFloats; ++i)
	{
		writer->write(&skeleton->m_referenceFloats[i], sizeof(hkReal));
	}

		/// Floating point track slots. Often used for auxiliary float data or morph target parameters etc.
		/// This defines the target when binding animations to a particular rig.
	int nfloatSlots = skeleton->m_floatSlots.getSize();
	writer->write(&nfloatSlots, sizeof(int));

	for (int i=0; i<nfloatSlots; ++i)
	{
		hkStringPtr name = skeleton->m_floatSlots[i];
		writer->write(name.cString(), name.getLength()+1);
	}
}

void write(hkStreamWriter *writer, hkaAnimation *anim)
{
		/// Returns the number of original samples / frames of animation.
	int numOriginalFrames = anim->getNumOriginalFrames();
		/// The length of the animation cycle in seconds
	hkReal duration = anim->m_duration;
		/// The number of bone tracks to be animated.
	int numTransforms = anim->m_numberOfTransformTracks;
		/// The number of float tracks to be animated
	int numFloats = anim->m_numberOfFloatTracks;

	writer->write(&numOriginalFrames, sizeof(int));
	writer->write(&duration, sizeof(hkReal));
	writer->write(&numTransforms, sizeof(int));
	writer->write(&numFloats, sizeof(int));

	hkLocalArray<hkQsTransform> transformOut(numTransforms);
	hkLocalArray<hkReal> floatsOut(numFloats);

	transformOut.setSize(numTransforms);
	floatsOut.setSize(numFloats);

	hkReal time = 0.0f;

	for (int f=0; f<numOriginalFrames; ++f, time = (hkReal)f * duration / (hkReal)(numOriginalFrames-1))
	{
			/// Get a subset of the first 'maxNumTracks' transform tracks (all tracks from 0 to maxNumTracks-1 inclusive), and the first 'maxNumFloatTracks' float tracks of a pose at a given time.
		anim->samplePartialTracks(time, numTransforms, transformOut.begin(), numFloats, floatsOut.begin(), HK_NULL);
		hkaSkeletonUtils::normalizeRotations(transformOut.begin(), numTransforms);

		writer->write(&time, sizeof(hkReal));

		for (int i=0; i<numTransforms; ++i)
		{
			write(writer, transformOut[i]);
		}

		for (int i=0; i<numFloats; ++i)
		{
			writer->write(&floatsOut[i], sizeof(hkReal));
		}
	}
}

void write(hkStreamWriter *writer, hkaAnimationBinding *binding)
{
		/// (Optional) The name of the skeleton from which the binding was constructed.
	hkStringPtr name = binding->m_originalSkeletonName;
	writer->write(name.cString(), name.getLength()+1);

		/// A handle to the animation bound to the skeleton.
	hkaAnimation *anim = binding->m_animation;

		/// A mapping from animation track indices to bone indices.
	int ntransformToBoneIndices = binding->m_transformTrackToBoneIndices.getSize();
	writer->write(&ntransformToBoneIndices, sizeof(int));

	for (int i=0; i<ntransformToBoneIndices; i++)
	{
		writer->write(&binding->m_transformTrackToBoneIndices[i], sizeof(hkInt16));
	}

		/// A mapping from animation float track indices to float slot indices.
	int nfloatTrackToFloatSlotIndices = binding->m_floatTrackToFloatSlotIndices.getSize();
	writer->write(&nfloatTrackToFloatSlotIndices, sizeof(int));

	for (int i=0; i<nfloatTrackToFloatSlotIndices; i++)
	{
		writer->write(&binding->m_floatTrackToFloatSlotIndices[i], sizeof(hkInt16));
	}
}

void dump(const char* filename, const char* destname)
{
	hkOstream deststream(destname);
	hkStreamWriter *writer = deststream.getStreamWriter();

	hkStringPtr head = "hkdump File Format, Version 1.0.0.0\n";
	writer->write(head.cString(), head.getLength());

	const unsigned int version = 0x01000000;
	writer->write(&version, sizeof(unsigned int));

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
				writer->write(&nskeletons, sizeof(int));

				if (nskeletons != 0)
				{
					hkaSkeleton *skeleton = animCont->m_skeletons[0];
					write(writer, skeleton);
				}

				int nanimations = animCont->m_animations.getSize();
				writer->write(&nanimations, sizeof(int));

				if (nanimations != 0)
				{
					hkaAnimation *animation = animCont->m_animations[0];
					write(writer, animation);
				}

				/*
				int nbindings = animCont->m_bindings.getSize();
				writer->write(&nbindings, sizeof(int));

				if (nbindings != 0)
				{
					hkaAnimationBinding *binding = animCont->m_bindings[0];
					write(writer, binding);
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
		puts("Usage: hkdump-bin.exe -o out.bin src.hkx");
		return 1;
	}

	if (ofile == NULL)
		ofile = "out.bin";

	// Need to have memory allocated for the solver. Allocate 1mb for it.
	hkMemoryRouter *memoryRouter = hkMemoryInitUtil::initDefault( hkMallocAllocator::m_defaultMallocAllocator, hkMemorySystem::FrameInfo(1024 * 1024) );
	hkBaseSystem::init( memoryRouter, errorReport );

	dump(ifile, ofile);

	// Shutdown
	hkBaseSystem::quit();
	hkMemoryInitUtil::quit();

	return 0;
}
