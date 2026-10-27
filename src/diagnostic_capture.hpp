#pragma once
#include "core.hpp"
#include <filesystem>
namespace ps1 {
// Caller pauses/joins the worker before taking the snapshot.
bool save_debug_images(const Bus& bus,const std::filesystem::path& log_path);
}
