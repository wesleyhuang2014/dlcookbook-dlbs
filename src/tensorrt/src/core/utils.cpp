/*
 (c) Copyright [2017] Hewlett Packard Enterprise Development LP
 
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
 
     http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/


#include "core/utils.hpp"

template<>
std::string S<bool>(const bool &t) { return (t ? "true" : "false"); }

void fill_random(std::vector<float>& vec) {
  std::random_device rnd_device;
  std::mt19937 mersenne_engine(rnd_device());
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  auto gen = std::bind(dist, mersenne_engine);
  std::generate(std::begin(vec), std::end(vec), gen);
}


std::string fs_utils::parent_dir(std::string dir) {
    if (dir.empty())
        return "";
    dir = normalize_path(dir);
    dir = dir.erase(dir.size() - 1);
    const auto pos = dir.find_last_of("/");
    if(pos == std::string::npos)
        return "";
    return dir.substr(0, pos);
}

int fs_utils::mk_dir(std::string dir, const mode_t mode) {
    struct stat sb;
    if (stat(dir.c_str(), &sb) != 0 ) {
        // Path does not exist
        const int ret = mk_dir(parent_dir(dir), mode);
        if(ret < 0)
            return ret;
        return mkdir(dir.c_str(), mode);
    }
    if (!S_ISDIR(sb.st_mode)) {
        // Path exists and is not a directory
        return -1;
    }
    // Path exists and is a directory.
    return 0;
}

std::string fs_utils::normalize_path(std::string dir) {
    const auto pos = dir.find_last_not_of("/");
    if (pos != std::string::npos && pos + 1 < dir.size())
        dir = dir.substr(0, pos + 1);
    dir += "/";
    return dir;
}

bool fs_utils::read_cache(const std::string& dir, std::vector<std::string>& fnames) {
    std::ifstream fstream(dir + "/" + "dlbs_image_cache");
    if (!fstream) return false;
    std::string fname;
    while (std::getline(fstream, fname))
        fnames.push_back(fname);
    return true;
}
    
bool fs_utils::write_cache(const std::string& dir, const std::vector<std::string>& fnames) {
    struct stat sb;
    const std::string cache_fname = dir + "/" + "dlbs_image_cache";
    if (stat(cache_fname.c_str(), &sb) == 0)
        return true;
    std::ofstream fstream(cache_fname.c_str());
    if (!fstream)
        return false;
    for (const auto& fname : fnames) {
        fstream << fname << std::endl;
    }
    return true;
}

void fs_utils::get_image_files(std::string dir, std::vector<std::string>& files, std::string subdir) {
    // Check that directory exists
    struct stat sb;
    if (stat(dir.c_str(), &sb) != 0) {
        std::cerr << "[get_image_files       ]: skipping path ('" << dir << "') - cannot stat directory (reason: " << get_errno() << ")" << std::endl;
        return;
    }
    if (!S_ISDIR(sb.st_mode)) {
        std::cerr << "[get_image_files       ]: skipping path ('" << dir << "') - not a directory" << std::endl;
        return;
    }
    // Scan this directory for files and other directories
    const std::string abs_path = dir + subdir;
    DIR *dir_handle = opendir(abs_path.c_str());
    if (dir_handle == nullptr)
        return;
    struct dirent *de(nullptr);
    while ((de = readdir(dir_handle)) != nullptr) {
        const std::string dir_item(de->d_name);
        if (dir_item == "." || dir_item == "..") {
            continue;
        }
        bool is_file(false), is_dir(false);
        if (de->d_type != DT_UNKNOWN) {
            is_file = de->d_type == DT_REG;
            is_dir = de->d_type == DT_DIR;
        } else {
            const std::string dir_item_path = dir + subdir + dir_item;
            if (stat(dir_item_path.c_str(), &sb) != 0) {
                continue;
            }
            is_file = S_ISREG(sb.st_mode);
            is_dir = S_ISDIR(sb.st_mode);
        }
        if (is_dir) {
            get_image_files(dir, files, subdir + dir_item + "/");
        } else if (is_file) {
            const auto pos = dir_item.find_last_of('.');
            if (pos != std::string::npos) {
                std::string fext = dir_item.substr(pos + 1);
                std::transform(fext.begin(), fext.end(), fext.begin(), ::tolower);
                if (fext == "jpg" || fext == "jpeg") {
                    files.push_back(subdir + dir_item);
                }
            }
        } {
        }
    }
    closedir(dir_handle);
}

void fs_utils::initialize_dataset(std::string& data_dir, std::vector<std::string>& files) {
    data_dir = fs_utils::normalize_path(data_dir);
    if (!fs_utils::read_cache(data_dir, files)) {
        std::cout << "[image_provider        ]: found " + S(files.size()) +  " image files in file system." << std::endl;
        fs_utils::get_image_files(data_dir, files);
        if (!files.empty()) {
            if (!fs_utils::write_cache(data_dir, files)) {
                std::cout << "[image_provider        ]: failed to write file cache." << std::endl;
            }
        }
    } else {
        std::cout << "[image_provider        ]: read " + S(files.size()) +  " from cache." << std::endl;
        if (files.empty()) { 
            std::cout << "[image_provider        ]: found empty cache file. Please, delete it and restart DLBS. " << std::endl;
        }
    }
    if (files.empty()) {
        std::cout << "[image_provider        ]: no input data found, exiting." << std::endl;
    }
    fs_utils::to_absolute_paths(data_dir, files);
}


std::ostream& operator<<(std::ostream &out, const running_average &ra) {
    std::cout << "running_average{size=" << ra.num_steps() << ", value=" << ra.value() << "}";
    return out;
}