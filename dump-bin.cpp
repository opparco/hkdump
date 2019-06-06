#include <iostream>

#define HK_CLASSES_FILE <Common/Serialize/Classlist/hkAnimationClasses.h>
#include <Common/Base/hkBase.h>

#include <Common/Base/Memory/System/Util/hkMemoryInitUtil.h>
#include <Common/Base/Memory/Allocator/Malloc/hkMallocAllocator.h>
#include <Common/Base/System/Io/IStream/hkIStream.h>

#include <Common/Base/Reflection/Registry/hkDefaultClassNameRegistry.h>

#include <Common/Serialize/Util/hkLoader.h>
#include <Common/Serialize/Util/hkSerializeUtil.h>
#include <Common/Serialize/Util/hkRootLevelContainer.h>

#include <Common/Compat/Deprecated/Packfile/Binary/hkBinaryPackfileReader.h>

// Animation
#include <Animation/Animation/hkaAnimationContainer.h>
#include <Animation/Animation/Rig/hkaSkeletonUtils.h>
#include <Animation/Animation/Motion/Default/hkaDefaultAnimatedReferenceFrame.h>

#pragma comment (lib, "hkBase.lib")
#pragma comment (lib, "hkSerialize.lib")
#pragma comment (lib, "hkSceneData.lib")
#pragma comment (lib, "hkInternal.lib")
#pragma comment (lib, "hkGeometryUtilities.lib")

#pragma comment (lib, "hkCompat.lib")

#pragma comment (lib, "hkcdCollide.lib")
#pragma comment (lib, "hkcdInternal.lib")

#pragma comment (lib, "hkaAnimation.lib")
#pragma comment (lib, "hkaInternal.lib")

void PlatformInit();
void PlatformFileSystemInit();

void HK_CALL errorReport(const char* msg, void* userContext)
{
	std::cerr << msg << std::endl;
}

// Define a useful macro for this demo - it allow us to detect a failure, print a message, and return early
#define RETURN_FAIL_IF(COND, MSG) \
	HK_MULTILINE_MACRO_BEGIN \
		if(COND) { HK_ERROR(0x53a6a026, MSG); return 1; } \
	HK_MULTILINE_MACRO_END

hkResource *hkSerializeLoadResource(hkStreamReader *reader)
{
	hkTypeInfoRegistry &defaultTypeRegistry = hkTypeInfoRegistry::getInstance();

	hkSerializeUtil::FormatDetails formatDetails;
	hkSerializeUtil::detectFormat( reader, formatDetails );
	hkBool32 isLoadable = hkSerializeUtil::isLoadable( reader );
	if (!isLoadable)
		return HK_NULL;

	if ( formatDetails.m_formatType != hkSerializeUtil::FORMAT_PACKFILE_BINARY )
		return HK_NULL;

	hkResource *resource = HK_NULL;
	{
		hkBinaryPackfileReader bpkreader;
		bpkreader.loadEntireFile( reader );
		bpkreader.finishLoadedObjects( defaultTypeRegistry );
		if ( hkPackfileData* pkdata = bpkreader.getPackfileData() )
		{
			resource = pkdata;
			resource->addReference();
		}
	}
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
		anim->samplePartialTracks(time, numTransforms, transformOut.begin(), numFloats, floatsOut.begin());
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

			/// The annotation tracks associated with this skeletal animation.
	const int numAnnotationTracks = anim->m_annotationTracks.getSize();
	writer->write(&numAnnotationTracks, sizeof(int));

	for (hkArray< class hkaAnnotationTrack >::const_iterator annotationTrack = anim->m_annotationTracks.begin(); annotationTrack != anim->m_annotationTracks.end(); ++annotationTrack)
	{
		const int numAnnotations = annotationTrack->m_annotations.getSize();
		writer->write(&numAnnotations, sizeof(int));

		for (hkArray< class hkaAnnotationTrack::Annotation >::const_iterator annotation = annotationTrack->m_annotations.begin(); annotation != annotationTrack->m_annotations.end(); ++annotation)
		{
			writer->write(&annotation->m_time, sizeof(hkReal));

			hkStringPtr text = annotation->m_text;
			writer->write(text.cString(), text.getLength()+1);
		}
	}

	/// Test for presence of extracted motion
	hkBool hasExtractedMotion = anim->hasExtractedMotion();
	writer->write(&hasExtractedMotion, sizeof(hkBool));
	if (hasExtractedMotion)
	{
		/// Get the extracted motion of this animation
		const hkaAnimatedReferenceFrame* extractedMotion = anim->getExtractedMotion();
		int frameType = extractedMotion->m_frameType;
		writer->write(&frameType, sizeof(int));
		switch (frameType)
		{
		case 1:
			{
				const hkaDefaultAnimatedReferenceFrame* defaultMotion = static_cast<const hkaDefaultAnimatedReferenceFrame*>(extractedMotion);

				/// Up vector.
				writer->write(&defaultMotion->m_up, sizeof(hkVector4));

				/// Forward vector.
				writer->write(&defaultMotion->m_forward, sizeof(hkVector4));

				/// The duration of the animated reference frame
				writer->write(&defaultMotion->m_duration, sizeof(hkReal));

				/// We store the motion track as a vector4 since we only need a translation and a rotational (w) component
				/// around the up direction.
				const int numReferenceFrameSamples = defaultMotion->m_referenceFrameSamples.getSize();
				writer->write(&numReferenceFrameSamples, sizeof(int));
				for (int i=0; i<numReferenceFrameSamples; i++)
				{
					writer->write(&defaultMotion->m_referenceFrameSamples[i], sizeof(hkVector4));
				}
			}
		}
	}
}

int dump(const char* filename, const char* destname)
{
	hkOstream deststream(destname);
	hkStreamWriter *writer = deststream.getStreamWriter();

	hkStringPtr head = "hkdump File Format, Version 3.0.2.0\n";
	writer->write(head.cString(), head.getLength());

	const unsigned int version = 0x03000200;
	writer->write(&version, sizeof(unsigned int));

	hkIstream stream(filename);
	hkStreamReader *reader = stream.getStreamReader();

	hkResource *resource = hkSerializeLoadResource(reader);

	RETURN_FAIL_IF( resource == HK_NULL, "Failed loading resource");
	{
		hkRootLevelContainer* rootCont = resource->getContents<hkRootLevelContainer>();
		RETURN_FAIL_IF( rootCont == HK_NULL, "Failed to get rootCont");

		hkaAnimationContainer *animCont = rootCont->findObject<hkaAnimationContainer>();
		RETURN_FAIL_IF( animCont == HK_NULL, "Failed to get animCont");

		int nskeletons = animCont->m_skeletons.getSize();
		writer->write(&nskeletons, sizeof(int));

		int nanimations = animCont->m_animations.getSize();
		writer->write(&nanimations, sizeof(int));

		if (nanimations != 0)
		{
			hkaAnimation *animation = animCont->m_animations[0];
			write(writer, animation);
		}
	}
	resource->removeReference();
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
		puts("Usage: hkdump-bin.exe -o out.bin src.hkx");
		return 1;
	}

	if (ofile == NULL)
		ofile = "out.bin";

	// Perfrom platform specific initialization for this demo - you should already have something similar in your own code.
	PlatformInit();

	// Need to have memory allocated for the solver. Allocate 1mb for it.
	hkMemoryRouter *memoryRouter = hkMemoryInitUtil::initDefault( hkMallocAllocator::m_defaultMallocAllocator, hkMemorySystem::FrameInfo(1024 * 1024) );
	hkBaseSystem::init( memoryRouter, errorReport );

	int ret = dump(ifile, ofile);

	// Shutdown
	hkBaseSystem::quit();
	hkMemoryInitUtil::quit();

	return ret;
}

#include <Common/Base/Config/hkProductFeaturesNoPatchesOrCompat.h>
#include <Common/Base/Config/hkProductFeatures.cxx>
#include <Common/Compat/Deprecated/Compat/hkCompat_None.cxx>

// Platform specific initialization

#include <Common/Base/System/Init/PlatformInit.cxx>
