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

void write(hkOstream& o, const hkVector4& v)
{
	float x = v(0);
	float y = v(1);
	float z = v(2);
	float w = v(3);

	o.printf(" %.6f %.6f %.6f %.6f\n", x, y, z, w);
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

	for (int f=0; f<numOriginalFrames; ++f, time = (hkReal)f * duration / (hkReal)(numOriginalFrames-1))
	{
		/// Get a subset of the first 'maxNumTracks' transform tracks (all tracks from 0 to maxNumTracks-1 inclusive), and the first 'maxNumFloatTracks' float tracks of a pose at a given time.
		anim->samplePartialTracks(time, numTransforms, transformOut.begin(), numFloats, floatsOut.begin());
		hkaSkeletonUtils::normalizeRotations(transformOut.begin(), numTransforms);

		o.printf("time: %.6f\n", time);

		for (int i=0; i<numTransforms; ++i)
		{
			o.printf("%d", i);
			write(o, transformOut[i]);
		}

		for (int i=0; i<numFloats; ++i)
		{
			o.printf("%d %.6f\n", i, floatsOut[i]);
		}
	}

	/// The annotation tracks associated with this skeletal animation.
	const int numAnnotationTracks = anim->m_annotationTracks.getSize();
	o.printf("numAnnotationTracks: %d\n", numAnnotationTracks);

	for (hkArray< class hkaAnnotationTrack >::const_iterator annotationTrack = anim->m_annotationTracks.begin(); annotationTrack != anim->m_annotationTracks.end(); ++annotationTrack)
	{
		const int numAnnotations = annotationTrack->m_annotations.getSize();
		o.printf("numAnnotations: %d\n", numAnnotations);

		for (hkArray< class hkaAnnotationTrack::Annotation >::const_iterator annotation = annotationTrack->m_annotations.begin(); annotation != annotationTrack->m_annotations.end(); ++annotation)
		{
			o.printf("time: %.6f", annotation->m_time);
			o << " text: " << annotation->m_text << "\n";
		}
	}

	/// Test for presence of extracted motion
	hkBool hasExtractedMotion = anim->hasExtractedMotion();
	o.printf("hasExtractedMotion: %d\n", hasExtractedMotion);
	if (hasExtractedMotion)
	{
		/// Get the extracted motion of this animation
		const hkaAnimatedReferenceFrame* extractedMotion = anim->getExtractedMotion();
		int frameType = extractedMotion->m_frameType;
		o.printf("frameType: %d\n", frameType);
		switch (frameType)
		{
		case 1:
			{
				const hkaDefaultAnimatedReferenceFrame* defaultMotion = static_cast<const hkaDefaultAnimatedReferenceFrame*>(extractedMotion);

				/// Up vector.
				o.printf("up:");
				write(o, defaultMotion->m_up);

				/// Forward vector.
				o.printf("forward:");
				write(o, defaultMotion->m_forward);

				/// The duration of the animated reference frame
				o.printf("duration: %.6f\n", defaultMotion->m_duration);

				/// We store the motion track as a vector4 since we only need a translation and a rotational (w) component
				/// around the up direction.
				const int numReferenceFrameSamples = defaultMotion->m_referenceFrameSamples.getSize();
				o.printf("numReferenceFrameSamples: %d\n", numReferenceFrameSamples);
				for (int i=0; i<numReferenceFrameSamples; i++)
				{
					o.printf("%d", i);
					write(o, defaultMotion->m_referenceFrameSamples[i]);
				}
			}
		}
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

int dump(const char* filename, const char* destname)
{
	hkOstream o(destname);

	o << "hkdump File Format, Version 3.0.2.0\n";

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
		o.printf("#skeletons: %d\n", nskeletons);

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
		puts("Usage: hkdump.exe -o out.txt src.hkx");
		return 1;
	}

	if (ofile == NULL)
		ofile = "out.txt";

	// Perfrom platform specific initialization for this demo - you should already have something similar in your own code.
	PlatformInit();

	// Need to have memory allocated for the solver. Allocate 1mb for it.
	hkMemoryRouter* memoryRouter = hkMemoryInitUtil::initDefault( hkMallocAllocator::m_defaultMallocAllocator, hkMemorySystem::FrameInfo(1024 * 1024) );

	hkBaseSystem::init( memoryRouter, errorReport );

	// Set up platform-specific file system info.
	PlatformFileSystemInit();

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
