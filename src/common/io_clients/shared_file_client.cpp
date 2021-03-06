/*
 * Copyright (C) 2019  SCS Lab <scs-help@cs.iit.edu>, Hariharan
 * Devarajan <hdevarajan@hawk.iit.edu>, Xian-He Sun <sun@iit.edu>
 *
 * This file is part of HFetch
 * 
 * HFetch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
//
// Created by hariharan on 3/19/19.
//

#include "shared_file_client.h"

ServerStatus SharedFileClient::Read(PosixFile &source, PosixFile &destination) {
    AutoTrace trace = AutoTrace("FileClient::Read",source,destination);
    std::string file_path=std::string(source.layer.layer_loc.c_str())+FILE_SEPARATOR+std::string(source.filename.c_str());
    FILE* fh = fopen(file_path.c_str(),"r");
    fseek(fh,source.segment.start,SEEK_SET);
    size_t size=source.segment.end-source.segment.start;
    char* data= static_cast<char *>(malloc(size));
    fread(data,size, 1,fh);
    destination.data.assign(data,size);
    fclose(fh);
    return SERVER_SUCCESS;
}

ServerStatus SharedFileClient::Write(PosixFile &source, PosixFile &destination) {
    AutoTrace trace = AutoTrace("FileClient::Write",source,destination);
    std::string file_path=std::string(destination.layer.layer_loc.c_str())+FILE_SEPARATOR+std::string(destination.filename.c_str());
    FILE* fh = fopen(file_path.c_str(),"r+");
    if(fh==NULL){
        fh = fopen(file_path.c_str(),"w+");
    }
    fseek(fh,destination.segment.start,SEEK_SET);
    fwrite(source.data.c_str(),destination.segment.end-destination.segment.start, 1,fh);
    fclose(fh);
    hasChanged.Put(destination.layer.id_,true);
    return SERVER_SUCCESS;
}

ServerStatus SharedFileClient::Delete(PosixFile file) {
    AutoTrace trace = AutoTrace("FileClient::Delete",file);
    hasChanged.Put(file.layer.id_,true);
    std::string file_path=std::string(file.layer.layer_loc.c_str())+FILE_SEPARATOR+std::string(file.filename.c_str());
    FILE* fh = fopen(file_path.c_str(),"r");
    size_t end = fseek(fh,0,SEEK_END);
    if(file.segment.end - file.segment.start + 1 - end == 0){
        remove(file_path.c_str());
        return SERVER_SUCCESS;
    }
    fseek(fh,0,SEEK_SET);
    void* data=malloc(end);
    fread(data,end, 1,fh);
    fclose(fh);
    fh = fopen(file_path.c_str(),"w+");
    if(file.segment.start > 0) fwrite(data, file.segment.start - 1, 1, fh);
    if(file.segment.end < end) fwrite((char*)data + file.segment.end, end - file.segment.end + 1, 1, fh);
    fclose(fh);
    hasChanged.Put(file.layer.id_,true);
    return SERVER_SUCCESS;
}

double SharedFileClient::GetCurrentUsage(Layer layer) {
    AutoTrace trace = AutoTrace("FileClient::GetCurrentUsage",layer);
    auto capacity_iter=layerCapacity.Get(layer.id_);
    if(capacity_iter.first){
        auto changed_iter=hasChanged.Get(layer.id_);
        if(!changed_iter.first || !changed_iter.second){
            return capacity_iter.second;
        }
    }
    std::string cmd="du -s "+std::string(layer.layer_loc.c_str())+" | awk {'print$1'}";
    FILE *fp;
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe=popen(cmd.c_str(), "r");
    while(!pipe){
        sleep(1);
        pipe=popen(cmd.c_str(), "r");
    }
    while (!feof(pipe)) {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }
    auto size=std::stoll(result)-4;
    pclose(pipe);
    layerCapacity.Put(layer.id_,size);
    hasChanged.Put(layer.id_,false);
    return size;
}
