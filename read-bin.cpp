#include <iostream>
#include <fstream>
#include <stdio.h>
#include <assert.h>

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/animation_builder.h"

#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"

using namespace ozz::animation::offline;

void read(std::istream& stream, ozz::math::Transform& transform)
{
	ozz::math::Float4 translation;
	ozz::math::Float4 rotation;
	ozz::math::Float4 scale;

	stream.read((char *)&translation, sizeof(ozz::math::Float4));
	stream.read((char *)&rotation, sizeof(ozz::math::Float4));
	stream.read((char *)&scale, sizeof(ozz::math::Float4));

	transform.translation.x = translation.x;
	transform.translation.y = translation.y;
	transform.translation.z = translation.z;

	transform.rotation.x = rotation.x;
	transform.rotation.y = rotation.y;
	transform.rotation.z = rotation.z;
	transform.rotation.w = rotation.w;

	transform.scale.x = scale.x;
	transform.scale.y = scale.y;
	transform.scale.z = scale.z;
}

void read(std::istream& stream, RawSkeleton *skeleton)
{
	{
		char buf[240];
		stream.getline(buf, 240, '\0');
		puts(buf);

		//skeleton->m_name = buf;
	}

	int nparentIndices;
	stream.read((char *)&nparentIndices, sizeof(int));
	printf("#parentIndices: %d\n", nparentIndices);
	//skeleton->m_parentIndices.setSize(nparentIndices);

	std::vector<RawSkeleton::Joint::Children::iterator> joint_vec;	// vector of joint ref
	joint_vec.resize(nparentIndices);

	std::vector<int> rootIndices;

	std::vector<std::vector<int>> childrenIndices;
	childrenIndices.resize(nparentIndices);

	for (int i=0; i<nparentIndices; ++i)
	{
		short parentIdx;
		stream.read((char *)&parentIdx, sizeof(short));
		//printf("%d %d\n", i, parentIdx);

		if (parentIdx == -1)
			rootIndices.push_back(i);
		else
			childrenIndices[parentIdx].push_back(i);
	}

	{
		auto sibling = &skeleton->roots;
		sibling->resize(rootIndices.size());
		auto p = sibling->begin();
		for (int f=0; f<rootIndices.size(); ++f)
		{
			int g = rootIndices[f];
			joint_vec[g] = p++;
		}
	}

	for (int i=0; i<childrenIndices.size(); ++i)
	{
		auto indices = childrenIndices[i];
		auto sibling = &joint_vec[i]->children;
		sibling->resize(indices.size());
		auto p = sibling->begin();
		for (int f=0; f<indices.size(); ++f)
		{
			int g = indices[f];
			joint_vec[g] = p++;
		}
	}

	int nbones;
	stream.read((char *)&nbones, sizeof(int));
	printf("#nbones: %d\n", nbones);
	//skeleton->m_bones.setSize(nbones);
	assert(nbones == nparentIndices);

	for (int i=0; i<nbones; ++i)
	{
		char buf[240];
		stream.getline(buf, 240, '\0');
		//puts(buf);

		joint_vec[i]->name = buf;
	}

	int nreferencePose;
	stream.read((char *)&nreferencePose, sizeof(int));
	printf("#nreferencePose: %d\n", nreferencePose);
	//skeleton->m_referencePose.setSize(nreferencePose);
	assert(nreferencePose == nparentIndices);

	for (int i=0; i<nreferencePose; ++i)
	{
		read(stream, joint_vec[i]->transform);	// read as ozz::math::Transform
	}

	int nreferenceFloats;
	stream.read((char *)&nreferenceFloats, sizeof(int));
	printf("#nreferenceFloats: %d\n", nreferenceFloats);
	//skeleton->m_referenceFloats.setSize(nreferenceFloats);	// ignore

	for (int i=0; i<nreferenceFloats; ++i)
	{
		float g;

		stream.read((char *)&g, sizeof(float));

		//skeleton->m_referenceFloats[i] = g;	// ignore
	}

	int nfloatSlots;
	stream.read((char *)&nfloatSlots, sizeof(int));
	printf("#nfloatSlots: %d\n", nfloatSlots);
	//skeleton->m_floatSlots.setSize(nfloatSlots);	// ignore
	assert(nreferenceFloats == nfloatSlots);

	for (int i=0; i<nfloatSlots; ++i)
	{
		char buf[240];
		stream.getline(buf, 240, '\0');
		//puts(buf);

		//skeleton->m_floatSlots[i] = buf;	// ignore
	}
}

void read(std::istream& stream, RawAnimation *anim)
{
	int numOriginalFrames;
	float duration;
	int numTransforms;
	int numFloats;

	stream.read((char *)&numOriginalFrames, sizeof(int));
	stream.read((char *)&duration, sizeof(float));
	stream.read((char *)&numTransforms, sizeof(int));
	stream.read((char *)&numFloats, sizeof(int));

	printf("numOriginalFrames: %d\n", numOriginalFrames);
	printf("duration: %.6f\n", duration);
	printf("numTransforms: %d\n", numTransforms);
	printf("numFloats: %d\n", numFloats);

	anim->duration = duration;
	anim->tracks.resize(numTransforms);

	for (int i=0; i<numTransforms; i++)
	{
		anim->tracks[i].translations.resize(numOriginalFrames);
		anim->tracks[i].rotations.resize(numOriginalFrames);
		anim->tracks[i].scales.resize(numOriginalFrames);
	}

	for (int f=0; f<numOriginalFrames; f++)
	{
		float time;
		stream.read((char *)&time, sizeof(float));
		//printf("time: %.6f\n", time);

		//TODO: track order
		//Skeleton jointsの並びはRawSkeleton jointsの並びと異なる
		for (int i=0; i<numTransforms; i++)
		{
			ozz::math::Transform transform;
			read(stream, transform);

			RawAnimation::TranslationKey& translationKey = anim->tracks[i].translations[f];
			translationKey.time = time;
			translationKey.value = transform.translation;

			RawAnimation::RotationKey& rotationKey = anim->tracks[i].rotations[f];
			rotationKey.time = time;
			rotationKey.value = transform.rotation;

			RawAnimation::ScaleKey& scaleKey = anim->tracks[i].scales[f];
			scaleKey.time = time;
			scaleKey.value = transform.scale;
		}

		int offFloats = f * numFloats;

		for (int i=0; i<numFloats; i++)
		{
			float g;

			stream.read((char *)&g, sizeof(float));

			//anim->m_floats[i + offFloats] = g;
		}
	}
}

int load(const char* filename)
{
	std::ifstream stream;
	stream.open(filename, std::ifstream::binary);

	char buf[240];
	stream.getline(buf, 240, '\n');
	puts(buf);

	unsigned int version;
	stream.read((char *)&version, sizeof(unsigned int));

	if (version != 0x01000000)
	{
		std::cerr << "Error: version mismatch! Abort.";
		return 100;
	}

	int nskeletons;
	stream.read((char *)&nskeletons, sizeof(int));
	printf("#skeletons: %d\n", nskeletons);

	if (nskeletons != 0)
	{
		RawSkeleton raw_skeleton;
		read(stream, &raw_skeleton);

		if (!raw_skeleton.Validate()) {
			std::cerr << "Error: raw skeleton is not valid.\n";
			return 101;
		}
		std::cerr << "raw skeleton is valid.\n";
		ozz::animation::offline::SkeletonBuilder builder;
		ozz::animation::Skeleton* skeleton = builder(raw_skeleton);

		puts("== skeleton joint names ==");
		for (auto i=skeleton->joint_names().begin; i!=skeleton->joint_names().end; ++i)
		{
			puts(*i);
		}

		ozz::memory::default_allocator()->Delete(skeleton);

		std::cerr << "Error: #skeletons should be 0 but " << nskeletons << "! Abort.";
		return 101;
	}

	int nanimations;
	stream.read((char*)&nanimations, sizeof(int));
	printf("#animations: %d\n", nanimations);

	if (nanimations != 1)
	{
		std::cerr << "Error: #animations should be 1 but " << nanimations << "! Abort.";
		return 101;
	}

	{
		RawAnimation raw_animation;
		read(stream, &raw_animation);

		if (!raw_animation.Validate()) {
			std::cerr << "Error: raw animation is not valid.\n";
			return 101;
		}
		std::cerr << "raw animation is valid.\n";

		ozz::animation::offline::AnimationBuilder builder;
		ozz::animation::Animation* animation = builder(raw_animation);

		puts("== animation buffer stat ==");
		printf("duration: %.6f\n", animation->duration() );
		printf("num_tracks: %d\n", animation->num_tracks() );
		printf("#translation keys: %lu\n", animation->translations().Count() );
		printf("#rotation keys: %lu\n", animation->rotations().Count() );
		printf("#scale keys: %lu\n", animation->scales().Count() );

		ozz::memory::default_allocator()->Delete(animation);
	}

	stream.close();
	return 0;
}

int main( int argc, char *argv[], char *envp[] )
{
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
			}
		}
		else
		{
			ifile = argv[i];
		}
	}

	if (ifile == NULL)
	{
		puts("Usage: read-bin.exe src.bin");
		return 1;
	}

	int ret = load(ifile);

	return ret;
}
