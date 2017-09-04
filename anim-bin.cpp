#include <iostream>
#include <fstream>
#include <map>
#include <stdio.h>
#include <assert.h>

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/animation_optimizer.h"

#include "ozz/animation/runtime/skeleton.h"

using namespace ozz::animation::offline;

/* idx to name for joint in raw_skeleton.joints */
std::vector<ozz::String::Std> m_raw_joint_names;

/* name to idx for joint in skeleton.joints */
std::map<ozz::String::Std, int> m_namemap;

ozz::animation::Skeleton* m_skeleton = NULL;

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

void read(std::istream& stream, RawSkeleton& skeleton)
{
	{
		char buf[240];
		stream.getline(buf, 240, '\0');
		puts(buf);

		//skeleton.m_name = buf;
	}

	int nparentIndices;
	stream.read((char *)&nparentIndices, sizeof(int));
	printf("#parentIndices: %d\n", nparentIndices);
	//skeleton.m_parentIndices.setSize(nparentIndices);

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
		auto sibling = &skeleton.roots;
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
	//skeleton.m_bones.setSize(nbones);
	assert(nbones == nparentIndices);

	m_raw_joint_names.resize(nbones);

	for (int i=0; i<nbones; ++i)
	{
		char buf[240];
		stream.getline(buf, 240, '\0');
		//puts(buf);

		joint_vec[i]->name = buf;
		m_raw_joint_names[i] = buf;
	}

	int nreferencePose;
	stream.read((char *)&nreferencePose, sizeof(int));
	printf("#nreferencePose: %d\n", nreferencePose);
	//skeleton.m_referencePose.setSize(nreferencePose);
	assert(nreferencePose == nparentIndices);

	for (int i=0; i<nreferencePose; ++i)
	{
		read(stream, joint_vec[i]->transform);	// read as ozz::math::Transform
	}

	int nreferenceFloats;
	stream.read((char *)&nreferenceFloats, sizeof(int));
	printf("#nreferenceFloats: %d\n", nreferenceFloats);
	//skeleton.m_referenceFloats.setSize(nreferenceFloats);	// ignore

	for (int i=0; i<nreferenceFloats; ++i)
	{
		float g;

		stream.read((char *)&g, sizeof(float));

		//skeleton.m_referenceFloats[i] = g;	// ignore
	}

	int nfloatSlots;
	stream.read((char *)&nfloatSlots, sizeof(int));
	printf("#nfloatSlots: %d\n", nfloatSlots);
	//skeleton.m_floatSlots.setSize(nfloatSlots);	// ignore
	assert(nreferenceFloats == nfloatSlots);

	for (int i=0; i<nfloatSlots; ++i)
	{
		char buf[240];
		stream.getline(buf, 240, '\0');
		//puts(buf);

		//skeleton.m_floatSlots[i] = buf;	// ignore
	}
}

int get_skeleton_joint_idx(int raw_joint_idx)
{
	ozz::String::Std raw_joint_name = m_raw_joint_names[raw_joint_idx];
	auto g = m_namemap.find(raw_joint_name);
	if ( g != m_namemap.end() )
		return g->second;
	else
		return -1;
}

void read(std::istream& stream, RawAnimation& anim)
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

	int ntracks = numTransforms;
	//TODO: min m_skeleton->num_joints()

	anim.duration = duration;
	anim.tracks.resize(ntracks);

	for (int i=0; i<ntracks; i++)
	{
		RawAnimation::JointTrack& track = anim.tracks[i];

		track.translations.resize(numOriginalFrames);
		track.rotations.resize(numOriginalFrames);
		track.scales.resize(numOriginalFrames);
	}

	for (int f=0; f<numOriginalFrames; f++)
	{
		float time;
		stream.read((char *)&time, sizeof(float));
		//printf("time: %.6f\n", time);

		for (int i=0; i<ntracks; i++)
		{
			ozz::math::Transform transform;
			read(stream, transform);

			int joint_idx = get_skeleton_joint_idx(i);

			if (joint_idx == -1)
				continue;

			RawAnimation::JointTrack& track = anim.tracks[joint_idx];

			RawAnimation::TranslationKey& translationKey = track.translations[f];
			translationKey.time = time;
			translationKey.value = transform.translation;

			RawAnimation::RotationKey& rotationKey = track.rotations[f];
			rotationKey.time = time;
			rotationKey.value = transform.rotation;

			RawAnimation::ScaleKey& scaleKey = track.scales[f];
			scaleKey.time = time;
			scaleKey.value = transform.scale;
		}

		int offFloats = f * numFloats;

		for (int i=0; i<numFloats; i++)
		{
			float g;

			stream.read((char *)&g, sizeof(float));

			//anim.m_floats[i + offFloats] = g;
		}
	}
}

int load(const char* filename, RawSkeleton& skeleton)
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
	assert(nskeletons != 0);

	read(stream, skeleton);

	if (!skeleton.Validate()) {
		std::cerr << "Error: raw skeleton is not valid.\n";
		return 101;
	}
	std::cerr << "raw skeleton is valid.\n";

	return 0;
}

int load(const char* filename, RawAnimation& anim)
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

	read(stream, anim);

	if (!anim.Validate()) {
		std::cerr << "Error: raw animation is not valid.\n";
		return 101;
	}
	std::cerr << "raw animation is valid.\n";

	return 0;
}

void write(std::ostream& stream, RawAnimation& anim)
{
	stream.write((char *)&anim.duration, sizeof(float));

	const int ntracks = anim.num_tracks();
	stream.write((char *)&ntracks, sizeof(int));

	for (int i=0; i<ntracks; ++i)
	{
		int joint_idx = get_skeleton_joint_idx(i);

		if (joint_idx == -1)
			continue;

		RawAnimation::JointTrack& track = anim.tracks[joint_idx];

		const int ntranslations = static_cast<int>(track.translations.size());
		stream.write((char *)&ntranslations, sizeof(int));
		for (int f=0; f<ntranslations; ++f)
		{
			stream.write((char *)&track.translations[f], sizeof(ozz::math::Float3));
		}

		const int nrotations = static_cast<int>(track.rotations.size());
		stream.write((char *)&nrotations, sizeof(int));
		for (int f=0; f<nrotations; ++f)
		{
			stream.write((char *)&track.rotations[f], sizeof(ozz::math::Quaternion));
		}

		const int nscales = static_cast<int>(track.scales.size());
		stream.write((char *)&nscales, sizeof(int));
		for (int f=0; f<nscales; ++f)
		{
			stream.write((char *)&track.scales[f], sizeof(ozz::math::Float3));
		}
	}
}

int save(const char* filename, RawAnimation& anim)
{
	std::ofstream stream;
	stream.open(filename, std::ofstream::binary);

	const std::string head = "hkanim File Format, Version 1.1.0.0\n";
	stream.write(head.data(), head.size());

	const unsigned int version = 0x01010000;
	stream.write((char *)&version, sizeof(unsigned int));

	const int nskeletons = 0;
	stream.write((char *)&nskeletons, sizeof(int));

	const int nanimations = 1;
	stream.write((char *)&nanimations, sizeof(int));

	write(stream, anim);

	return 0;
}

void quit()
{
	m_namemap.clear();
	m_raw_joint_names.clear();
	ozz::memory::default_allocator()->Delete(m_skeleton);
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

	RawSkeleton raw_skeleton;
	int ret;
	ret = load("skeleton.bin", raw_skeleton);
	if (ret != 0)
	{
		// m_skeleton is not allocated
		return 101;
	}

	SkeletonBuilder builder;
	m_skeleton = builder(raw_skeleton);	// allocate

	//puts("== skeleton joint names ==");
	for (auto i=m_skeleton->joint_names().begin; i!=m_skeleton->joint_names().end; ++i)
	{
		//puts(*i);
		m_namemap[*i] = i - m_skeleton->joint_names().begin;
	}

	RawAnimation raw_animation;
	ret = load(ifile, raw_animation);
	if (ret != 0)
	{
		quit();	// delete m_skeleton
		return ret;
	}

	{
		// Stores the optimizer in order to expose its parameters.
		AnimationOptimizer optimizer;

		// Translation optimization tolerance, defined as the distance between two
		// translation values in meters.
		// default: optimizer.translation_tolerance = 1e-3f;
		optimizer.translation_tolerance = 1e-1f;

		// Rotation optimization tolerance, ie: the angle between two rotation values
		// in radian.
		// default: optimizer.rotation_tolerance = .1f * ozz::math::kPi / 180.f;

		// Scale optimization tolerance, ie: the norm of the difference of two scales.
		// default: optimizer.scale_tolerance = 1e-3f;
		optimizer.scale_tolerance = 1e-1f;

		// Hierarchical translation optimization tolerance, ie: the maximum error
		// (distance) that an optimization on a joint is allowed to generate on its
		// whole child hierarchy.
		// default: optimizer.hierarchical_tolerance = 1e-3f;
		optimizer.hierarchical_tolerance = 1e-1f;

		RawAnimation optimized_animation;
		if (!optimizer(raw_animation, *m_skeleton, &optimized_animation))
		{
			quit();	// delete m_skeleton
			return 103;	// failed to optimize
		}
		save("out.ani", optimized_animation);
	}

	quit();	// delete m_skeleton
	return ret;
}
