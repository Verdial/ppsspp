// Copyright (c) 2016- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <mutex>
#include <string>

#include "Common/File/VFS/VFS.h"
#include "Common/GPU/thin3d.h"
#include "Common/Log.h"

struct ReplacedLevelsCache;
class TextureReplacer;
class LimitedWaitable;

// These must match the constants in TextureCacheCommon.
enum class ReplacedTextureAlpha {
	UNKNOWN = 0x04,
	FULL = 0x00,
};

// For forward compatibility, we specify the hash.
enum class ReplacedTextureHash {
	QUICK,
	XXH32,
	XXH64,
};

enum class ReplacedImageType {
	PNG,
	ZIM,
	DDS,
	INVALID,
};

static const int MAX_REPLACEMENT_MIP_LEVELS = 12;  // 12 should be plenty, 8 is the max mip levels supported by the PSP.

// Metadata about a given texture level.
struct ReplacedTextureLevel {
	int w = 0;
	int h = 0;
	Path file;

	// To be able to reload, we need to be able to reopen, unfortunate we can't use zip_file_t.
	// TODO: This really belongs on the level in the cache, not in the individual ReplacedTextureLevel objects.
	VFSFileReference *fileRef = nullptr;
};

enum class ReplacementState : uint32_t {
	UNINITIALIZED,
	POPULATED,  // We located the texture files but have not started the thread.
	PENDING,
	NOT_FOUND,  // Also used on error loading the images.
	ACTIVE,
	CANCEL_INIT,
};

const char *StateString(ReplacementState state);

struct GPUFormatSupport {
	bool bc123;
	bool astc;
	bool bc7;
	bool etc2;
};

struct ReplacementDesc {
	int newW;
	int newH;
	uint64_t cachekey;
	uint32_t hash;
	int w;
	int h;
	std::string hashfiles;
	Path basePath;
	bool foundAlias;
	std::vector<std::string> filenames;
	std::string logId;
	ReplacedLevelsCache *cache;
	GPUFormatSupport formatSupport;
};

struct ReplacedLevelsCache {
	Draw::DataFormat fmt = Draw::DataFormat::UNDEFINED;
	std::mutex lock;
	std::vector<std::vector<uint8_t>> data;
	double lastUsed = 0.0;
};

// These aren't actually all replaced, they can also represent a placeholder for a not-found
// replacement (state_ == NOT_FOUND).
struct ReplacedTexture {
	~ReplacedTexture();

	inline ReplacementState State() const {
		return state_;
	}

	void SetState(ReplacementState state) {
		_dbg_assert_(state != state_);
#ifdef _DEBUG
		// WARN_LOG(G3D, "Texture %s changed state from %s to %s", logId_.c_str(), StateString(state_), StateString(state));
#endif
		state_ = state;
	}

	void GetSize(int level, int *w, int *h) const {
		_dbg_assert_(State() == ReplacementState::ACTIVE);
		_dbg_assert_(level < levels_.size());
		*w = levels_[level].w;
		*h = levels_[level].h;
	}

	int GetLevelDataSize(int level) const {
		_dbg_assert_(State() == ReplacementState::ACTIVE);
		return (int)levelData_->data[level].size();
	}

	int NumLevels() const {
		_dbg_assert_(State() == ReplacementState::ACTIVE);
		return (int)levels_.size();
	}

	Draw::DataFormat Format() const {
		_dbg_assert_(State() == ReplacementState::ACTIVE);
		return fmt;
	}

	u8 AlphaStatus() const {
		return (u8)alphaStatus_;
	}

	bool IsReady(double budget);
	bool CopyLevelTo(int level, void *out, int rowPitch);

	void FinishPopulate(ReplacementDesc *desc);
	std::string logId_;

private:
	void Prepare(VFSBackend *vfs);
	bool LoadLevelData(ReplacedTextureLevel &info, int level, Draw::DataFormat *pixelFormat);
	void PurgeIfOlder(double t);

	std::vector<ReplacedTextureLevel> levels_;
	ReplacedLevelsCache *levelData_ = nullptr;

	ReplacedTextureAlpha alphaStatus_ = ReplacedTextureAlpha::UNKNOWN;
	double lastUsed_ = 0.0;
	LimitedWaitable *threadWaitable_ = nullptr;
	std::mutex mutex_;
	Draw::DataFormat fmt = Draw::DataFormat::UNDEFINED;  // NOTE: Right now, the only supported format is Draw::DataFormat::R8G8B8A8_UNORM.

	std::atomic<ReplacementState> state_ = ReplacementState::UNINITIALIZED;

	VFSBackend *vfs_ = nullptr;
	ReplacementDesc *desc_ = nullptr;

	friend class TextureReplacer;
	friend class ReplacedTextureTask;
};
