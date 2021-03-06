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

#ifndef HFETCH_DATA_STRUCTURE_H
#define HFETCH_DATA_STRUCTURE_H

#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <src/common/constants.h>
#include <src/common/enumerations.h>
#include <rpc/msgpack.hpp>
#include <math.h>
#include <boost/interprocess/containers/string.hpp>

namespace bip=boost::interprocess;

/* Simplified Layer structure */
typedef struct LayerInfo {
    char mount_point_[256];
    float capacity_mb_;
    float bandwidth;
    bool is_memory;
    bool is_local;
    bool direct_io;
} LayerInfo;

/* Input args structure for both lib and server */

typedef struct InputArgs{
    size_t io_size_mb;
    char pfs_path[256];
    int layer_count_;
    LayerInfo* layers;
    size_t iteration_;
    size_t direct_io_;
    int ranks_per_server_;
    size_t num_workers;
    int max_files;
    int num_servers;
    int comm_threads_per_server;
    bool is_logging;
    char log_path[256];


} InputArgs;


/* fixed size char for better inter process communications */
typedef struct CharStruct{
private:
    char value[MAX_STRING_LENGTH];
public:
    CharStruct(){}
    CharStruct(std::string data_){
        sprintf(this->value,"%s",data_.c_str());
    }
    CharStruct(char* data_, size_t size){
        sprintf(this->value,"%s",data_);
    }
    const char* c_str() const{
        return value;
    }

    char* data(){
        return value;
    }
    const size_t size() const{
        return strlen(value);
    }
    /* equal operator for comparing two Matrix. */
    bool operator==(const CharStruct &o) const {
        return strcmp(value,o.value)==0;
    }
} CharStruct;

/* represents the file segment data structure and all its required methods */
typedef struct Segment{
    long start;
    long end;
    Segment():start(0),end(0){}
    Segment(size_t start_, size_t end_):start(start_),end(end_){} /* Parameterized constructor */
    Segment(const Segment &other) : start(other.start),end(other.end) {} /* copy constructor */
    Segment(Segment &&other) : start(other.start),end(other.end) {} /* move constructor*/
    /* Assignment Operator */
    Segment &operator=(const Segment &other) {
        start=other.start;
        end=other.end;
        return *this;
    }

    /* less than operator for comparing two Segment. */
    bool operator<(const Segment &o) const {
        return o.start > start;
    }

    /* greater than operator for comparing two Segment. */
    bool operator>(const Segment &o) const {
        return o < *this;
    }

    /* equal operator for comparing two Segment. */
    bool operator==(const Segment &o) const {
        return start == o.start && end == o.end;
    }

    /* not equal operator for comparing two Segment. */
    bool operator!=(const Segment &o) const {
        return !(*this == o);
    }


    /**
     * Methods
     */
    long GetSize() const{
         return end - start + 1;
     }
    /**
     * Checks if the given current Segment contains the given Segment.
     *
     * @param o, given Segment
     * @return true if it contains else false.
     */
    bool Contains(const Segment &o) const {
        return (start <= o.start && end >= o.end) ||
                (start <= o.end && end >= o.end);
    }

    /**
     * This method calculates A - B of Segment. A is this Segment
     * @param o, B Segment
     * @return vectors of Segment which are left over from the substraction.
     */
    std::vector<Segment> Substract(const Segment &o) const {
        std::vector<Segment> left_over = std::vector<Segment>();
        Segment myinstance(*this);
        if (start < o.start && end >= o.end) {
            Segment new_segment(myinstance);
            new_segment.end = o.start - 1;
            left_over.push_back(new_segment);
            myinstance.start = o.start;
        }
        if (start <= o.end && end > o.end) {
            Segment new_segment(myinstance);
            new_segment.start = o.end + 1;
            left_over.push_back(new_segment);
            myinstance.end = o.end;
        }
        return left_over;
    }

    /**
     * Calculates the intersection of two Segment.
     * @param o, is the given Segment
     * @return intersection of two Segment.
     */
    Segment Intersect(Segment &o) const {
        Segment intersection(*this);
        if (start < o.start) {
            intersection.start = o.start;
        } else {
            intersection.start = start;
        }
        if (end < o.end) {
            intersection.end = end;
        } else {
            intersection.end = o.end;
        }
        return intersection;
    }

}Segment;

/* represents the segment score */

typedef struct SegmentScore{
    size_t frequency;
    double lrf;
    ScoreType type;
    SegmentScore():frequency(),lrf(),type(DEFAULT_SCORE_TYPE){}
    SegmentScore(const SegmentScore &other) : frequency(other.frequency),lrf(other.lrf),type(other.type){} /* copy constructor */
    SegmentScore(SegmentScore &&other) : frequency(other.frequency),lrf(other.lrf),type(other.type){} /* move constructor*/
    double GetScore() const{
        switch (type){
            case ScoreType ::FREQUENCY_SCORE:{
                return frequency;
            }
            case ScoreType ::LRF_SCORE:{
                return lrf;
            }
        }
        return frequency;//pow(0.5, (LAMDA_FOR_SCORE * time/1000.0));
    }
    /* Assignment Operator */
    SegmentScore &operator=(const SegmentScore &other) {
        frequency=other.frequency;
        lrf=other.lrf;
        type=other.type;
        return *this;
    }
    /* less than operator for comparing two SegmentScore. */
    bool operator<(const SegmentScore &o) const {
        return o.GetScore() < GetScore();
    }

    /* greater than operator for comparing two SegmentScore. */
    bool operator>(const SegmentScore &o) const {
        return o.GetScore() > GetScore();
    }

    /* equal operator for comparing two SegmentScore. */
    bool operator==(const SegmentScore &o) const {
        return o.GetScore() == GetScore();
    }

    /* not equal operator for comparing two SegmentScore. */
    bool operator!=(const SegmentScore &o) const {
        return o.GetScore() != GetScore();
    }
    /**
     * Checks if the given current Segment contains the given Segment.
     *
     * @param o, given Segment
     * @return true if it contains else false.
     */
    bool Contains(const SegmentScore &o) const {
        return GetScore() == o.GetScore();
    }

} SegmentScore;

/* represents an event in the system */
typedef struct Event{
    CharStruct filename;
    Segment segment;
    EventType event_type;
    EventSource source;
    uint8_t layer_index;
    HTime time;
    Event():filename(),segment(),event_type(),source(),layer_index(),time(){}
    Event(const Event &other) : filename(other.filename),segment(other.segment),event_type(other.event_type),source(other.source),layer_index(other.layer_index),time(other.time) {} /* copy constructor */
    Event(Event &&other) : filename(other.filename),segment(other.segment),event_type(other.event_type),source(other.source),layer_index(other.layer_index),time(other.time) {} /* move constructor*/
    /* Assignment Operator */
    Event &operator=(const Event &other) {
        filename=other.filename;
        segment=other.segment;
        event_type=other.event_type;
        source=other.source;
        layer_index=other.layer_index;
        time=other.time;
        return *this;
    }
} Event;

/* represents the various hierarchical layers supported hfetch platform. */
struct Layer {
    /**
     * Attributes
     */
    static Layer *FIRST; /* define enum for First layer */
    static Layer *LAST; /* define enum for Last Layer */
    uint8_t id_; /* id of the layer. */
    float capacity_mb_; /* capacity in Megabytes of the layer. */
    CharStruct layer_loc; /* location of the layer. */
    float bandwidth_mbps_; /* bandwidth in Megabytes per second. */
    Layer *next; /* pointer to next layer. */
    Layer *previous; /* pointer to previous layer. */
    bool direct_io;
    IOClientType io_client_type; /* Type of IO Client to use for the layer*/
    /**
     * Constructors
     */
    Layer():layer_loc() {} /* default constructor */
    Layer(const Layer &other) :
            id_(other.id_),
            capacity_mb_(other.capacity_mb_),
            layer_loc(other.layer_loc),
            bandwidth_mbps_(other.bandwidth_mbps_),
            next(other.next),
            previous(other.previous),
            direct_io(other.direct_io),
            io_client_type(other.io_client_type) {} /* copy constructor */
    /* parameterized constructor */
    Layer(uint8_t id){
        Layer* current=Layer::FIRST;
        while(current != nullptr){
            if(current->id_ == id){
                id_=current->id_;
                capacity_mb_=current->capacity_mb_;
                layer_loc=current->layer_loc;
                next=current->next;
                previous=current->previous;
                bandwidth_mbps_=current->bandwidth_mbps_;
                io_client_type=current->io_client_type;
                break;
            }
            current=current->next;
        }
    }
    Layer(Layer *next,
          Layer *previous,
          bool is_present,
          uint8_t id,
          float capacity,
          CharStruct layer_loc,
          float bandwidth,
          IOClientType io_client_type) :
            id_(id),
            capacity_mb_(capacity),
            layer_loc(layer_loc),
            next(next),
            previous(previous),
            bandwidth_mbps_(bandwidth),
            io_client_type(io_client_type) {
    }

    /**
     * Operators
     */
    /* equal operations for two layers. */
    bool operator==(const Layer &o) const {
        return id_ == o.id_;
    }

    /* not-equal operations for two layers. */
    bool operator!=(const Layer &o) const {
        return id_ != o.id_;
    }
};

/* represents a complete file structure */
typedef struct PosixFile{
    CharStruct filename;
    Segment segment;
    Layer layer;
    std::string data;
    PosixFile():filename(),segment(),layer(*Layer::LAST),data(){}
    PosixFile(const PosixFile &other) : filename(other.filename),segment(other.segment),layer(other.layer),data(other.data) {} /* copy constructor */
    PosixFile(PosixFile &&other) : filename(other.filename),segment(other.segment),layer(other.layer),data(other.data) {} /* move constructor*/
    /* Assignment Operator */
    PosixFile &operator=(const PosixFile &other) {
        filename=other.filename;
        segment=other.segment;
        layer=other.layer;
        data=other.data;
        return *this;
    }

    long GetSize() const{
        return segment.GetSize();
    }
} PosixFile;

/**
 * Hash Values
 */
namespace std {
    template<>
    struct hash<CharStruct> {
        size_t operator()(const CharStruct &k) const {
            std::string val(k.c_str());
            return std::hash<std::string>()(val);
        }
    };
    template<>
    struct hash<Segment> {
        size_t operator()(const Segment &k) const {
            size_t hash_val = hash<size_t >()(k.start);
            hash_val ^= hash<size_t>()(k.end);
            return hash_val;
        }
    };
    template<>
    struct hash<SegmentScore> {
        size_t operator()(const SegmentScore &k) const {
            size_t hash_val = hash<size_t >()(k.frequency);
            hash_val ^= hash<HTime>()(k.lrf);
            return hash_val;
        }
    };

}

/**
 * MSGPACK Serialization
 */

namespace clmdep_msgpack {
    MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
            namespace adaptor {
                namespace mv1=clmdep_msgpack::v1;
                template<>
                struct convert<CharStruct> {
                    mv1::object const& operator()(mv1::object const& o, CharStruct& input) const {
                        std::string v=std::string();
                        v.assign(o.via.str.ptr, o.via.str.size);
                        input=CharStruct(v);
                        return o;
                    }
                };

                template<>
                struct pack<CharStruct> {
                    template <typename Stream>
                    packer<Stream>& operator()(mv1::packer<Stream>& o, CharStruct const& input) const {
                        uint32_t size = checked_get_container_size(input.size());
                        o.pack_str(size);
                        o.pack_str_body(input.c_str(), size);
                        return o;
                    }
                };

                template <>
                struct object_with_zone<CharStruct> {
                    void operator()(mv1::object::with_zone& o, CharStruct const& input) const {
                        uint32_t size = checked_get_container_size(input.size());
                        o.type = clmdep_msgpack::type::STR;
                        char* ptr = static_cast<char*>(o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
                        o.via.str.ptr = ptr;
                        o.via.str.size = size;
                        std::memcpy(ptr, input.c_str(), input.size());
                    }
                };
                template<>
                struct convert<Segment> {
                    mv1::object const& operator()(mv1::object const& o, Segment& input) const {
                        input.start = o.via.array.ptr[0].as<long>();
                        input.end = o.via.array.ptr[1].as<long>();
                        return o;
                    }
                };

                template<>
                struct pack<Segment> {
                    template <typename Stream>
                    packer<Stream>& operator()(mv1::packer<Stream>& o, Segment const& input) const {
                        // packing member variables as an array.
                        o.pack_array(2);
                        o.pack(input.start);
                        o.pack(input.end);
                        return o;
                    }
                };

                template <>
                struct object_with_zone<Segment> {
                    void operator()(mv1::object::with_zone& o, Segment const& input) const {
                        o.type = type::ARRAY;
                        o.via.array.size = 2;
                        o.via.array.ptr = static_cast<clmdep_msgpack::object*>(o.zone.allocate_align(sizeof(mv1::object) * o.via.array.size, MSGPACK_ZONE_ALIGNOF(mv1::object)));
                        o.via.array.ptr[0] = mv1::object(input.start, o.zone);
                        o.via.array.ptr[1] = mv1::object(input.end, o.zone);
                    }
                };

                template<>
                struct convert<SegmentScore> {
                    mv1::object const& operator()(mv1::object const& o, SegmentScore& input) const {
                        input.frequency = o.via.array.ptr[0].as<size_t>();
                        input.lrf = o.via.array.ptr[1].as<double>();
                        return o;
                    }
                };

                template<>
                struct pack<SegmentScore> {
                    template <typename Stream>
                    packer<Stream>& operator()(mv1::packer<Stream>& o, SegmentScore const& input) const {
                        // packing member variables as an array.
                        o.pack_array(2);
                        o.pack(input.frequency);
                        o.pack(input.lrf);
                        return o;
                    }
                };

                template <>
                struct object_with_zone<SegmentScore> {
                    void operator()(mv1::object::with_zone& o, SegmentScore const& input) const {
                        o.type = type::ARRAY;
                        o.via.array.size = 2;
                        o.via.array.ptr = static_cast<clmdep_msgpack::object*>(o.zone.allocate_align(sizeof(mv1::object) * o.via.array.size, MSGPACK_ZONE_ALIGNOF(mv1::object)));
                        o.via.array.ptr[0] = mv1::object(input.frequency, o.zone);
                        o.via.array.ptr[1] = mv1::object(input.lrf, o.zone);
                    }
                };


                template<>
                struct convert<Event> {
                    mv1::object const& operator()(mv1::object const& o, Event& input) const {
                        input.filename = o.via.array.ptr[0].as<CharStruct>();
                        input.segment = o.via.array.ptr[1].as<Segment>();
                        input.event_type = o.via.array.ptr[2].as<EventType>();
                        input.source = o.via.array.ptr[3].as<EventSource>();
                        input.layer_index = o.via.array.ptr[4].as<uint8_t>();
                        input.time = o.via.array.ptr[5].as<HTime>();
                        return o;
                    }
                };

                template<>
                struct pack<Event> {
                    template <typename Stream>
                    packer<Stream>& operator()(mv1::packer<Stream>& o, Event const& input) const {
                        // packing member variables as an array.
                        o.pack_array(6);
                        o.pack(input.filename);
                        o.pack(input.segment);
                        o.pack(input.event_type);
                        o.pack(input.source);
                        o.pack(input.layer_index);
                        o.pack(input.time);
                        return o;
                    }
                };

                template <>
                struct object_with_zone<Event> {
                    void operator()(mv1::object::with_zone& o, Event const& input) const {
                        o.type = type::ARRAY;
                        o.via.array.size = 6;
                        o.via.array.ptr = static_cast<clmdep_msgpack::object*>(o.zone.allocate_align(sizeof(mv1::object) * o.via.array.size, MSGPACK_ZONE_ALIGNOF(mv1::object)));
                        o.via.array.ptr[0] = mv1::object(input.filename, o.zone);
                        o.via.array.ptr[1] = mv1::object(input.segment, o.zone);
                        o.via.array.ptr[2] = mv1::object(input.event_type, o.zone);
                        o.via.array.ptr[3] = mv1::object(input.source, o.zone);
                        o.via.array.ptr[4] = mv1::object(input.layer_index, o.zone);
                        o.via.array.ptr[5] = mv1::object(input.time, o.zone);
                    }
                };

                template<>
                struct convert<PosixFile> {
                    mv1::object const& operator()(mv1::object const& o, PosixFile& input) const {
                        input.filename = o.via.array.ptr[0].as<CharStruct>();
                        input.segment = o.via.array.ptr[1].as<Segment>();
                        input.layer = Layer(o.via.array.ptr[2].as<uint8_t>());
                        input.data = o.via.array.ptr[3].as<std::string>();
                       /* std::string s=o.via.array.ptr[3].as<std::string>();
                        if(s.size()>0){
                            input.data= static_cast<char *>(malloc(s.size() + 1));
                            strcpy(input.data,s.data());
                        }*/
                        return o;
                    }
                };

                template<>
                struct pack<PosixFile> {
                    template <typename Stream>
                    packer<Stream>& operator()(mv1::packer<Stream>& o, PosixFile const& input) const {
                        // packing member variables as an array.
                        o.pack_array(4);
                        o.pack(input.filename);
                        o.pack(input.segment);
                        o.pack(input.layer.id_);
                        o.pack(input.data);
                        /*if(input.data !=NULL) o.pack(std::string(input.data));
                        else o.pack(std::string());*/
                        return o;
                    }
                };

                template <>
                struct object_with_zone<PosixFile> {
                    void operator()(mv1::object::with_zone& o, PosixFile const& input) const {
                        o.type = type::ARRAY;
                        o.via.array.size = 4;
                        o.via.array.ptr = static_cast<clmdep_msgpack::object*>(o.zone.allocate_align(sizeof(mv1::object) * o.via.array.size, MSGPACK_ZONE_ALIGNOF(mv1::object)));
                        o.via.array.ptr[0] = mv1::object(input.filename, o.zone);
                        o.via.array.ptr[1] = mv1::object(input.segment, o.zone);
                        o.via.array.ptr[2] = mv1::object(input.layer.id_, o.zone);
                        o.via.array.ptr[3] = mv1::object(input.data, o.zone);
                        /*if(input.data !=NULL) o.via.array.ptr[3] = mv1::object(std::string(input.data), o.zone);
                        else o.via.array.ptr[3] = mv1::object(std::string(), o.zone);*/
                    }
                };

                template <>
                struct convert<bip::string> {
                    clmdep_msgpack::object const& operator()(clmdep_msgpack::object const& o, bip::string& v) const {
                        switch (o.type) {
                            case clmdep_msgpack::type::BIN:
                                v.assign(o.via.bin.ptr, o.via.bin.size);
                                break;
                            case clmdep_msgpack::type::STR:
                                v.assign(o.via.str.ptr, o.via.str.size);
                                break;
                            default:
                                throw clmdep_msgpack::type_error();
                                break;
                        }
                        return o;
                    }
                };


                template <>
                struct pack<bip::string> {
                    template <typename Stream>
                    clmdep_msgpack::packer<Stream>& operator()(clmdep_msgpack::packer<Stream>& o, const bip::string& v) const {
                        uint32_t size = checked_get_container_size(v.size());
                        o.pack_str(size);
                        o.pack_str_body(v.data(), size);
                        return o;
                    }
                };

                template <>
                struct object<bip::string> {
                    void operator()(clmdep_msgpack::object& o, const bip::string& v) const {
                        uint32_t size = checked_get_container_size(v.size());
                        o.type = clmdep_msgpack::type::STR;
                        o.via.str.ptr = v.data();
                        o.via.str.size = size;
                    }
                };

                template <>
                struct object_with_zone<bip::string> {
                    void operator()(clmdep_msgpack::object::with_zone& o, const bip::string& v) const {
                        uint32_t size = checked_get_container_size(v.size());
                        o.type = clmdep_msgpack::type::STR;
                        char* ptr = static_cast<char*>(o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
                        o.via.str.ptr = ptr;
                        o.via.str.size = size;
                        std::memcpy(ptr, v.data(), v.size());
                    }
                };
            }
    }
}


/**
 * Custom Iterator
 */

template <std::size_t I>
struct wrapper
{
    static constexpr std::size_t n = I;
};

template <class Func, std::size_t ...Is>
constexpr void static_for_impl( Func &&f, std::index_sequence<Is...> )
{
    ( f( wrapper<Is>{} ),... );
}

template <std::size_t N, class Func>
constexpr void static_for( Func &&f )
{
    static_for_impl( f, std::make_index_sequence<N>{ } );
}

/**
 * Outstream conversions
 */

std::ostream &operator<<(std::ostream &os, char const *m);
template <typename T,typename O>
std::ostream &operator<<(std::ostream &os, std::pair<T,O> const m){
    return os   << "{TYPE:pair," << "first:" << m.first << ","
                << "second:" << m.second << "}";
}
std::ostream &operator<<(std::ostream &os, uint8_t const &m);
std::ostream &operator<<(std::ostream &os, LayerInfo const &m);
std::ostream &operator<<(std::ostream &os, InputArgs const &m);
std::ostream &operator<<(std::ostream &os, CharStruct const &m);
std::ostream &operator<<(std::ostream &os, Segment const &m);
std::ostream &operator<<(std::ostream &os, SegmentScore const &m);
std::ostream &operator<<(std::ostream &os, Event const &m);
std::ostream &operator<<(std::ostream &os, Layer const &m);
std::ostream &operator<<(std::ostream &os, PosixFile const &m);
template <typename T>
std::ostream &operator<<(std::ostream &os, std::vector<T> const &ms){
    os << "[";
    for(auto m:ms){
        os <<m<<",";
    }
    os << "]";
    return os;
}


#endif //HFETCH_DATA_STRUCTURE_H
