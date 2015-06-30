// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: problem.proto

#ifndef PROTOBUF_problem_2eproto__INCLUDED
#define PROTOBUF_problem_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 2005000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 2005000 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/unknown_field_set.h>
#include "dna.pb.h"
#include "answer.pb.h"
// @@protoc_insertion_point(includes)

namespace ProblemBuffers {

// Internal implementation detail -- do not call these.
void  protobuf_AddDesc_problem_2eproto();
void protobuf_AssignDesc_problem_2eproto();
void protobuf_ShutdownFile_problem_2eproto();

class Problem;
class ProblemSettings;

// ===================================================================

class Problem : public ::google::protobuf::Message {
 public:
  Problem();
  virtual ~Problem();

  Problem(const Problem& from);

  inline Problem& operator=(const Problem& from) {
    CopyFrom(from);
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _unknown_fields_;
  }

  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return &_unknown_fields_;
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const Problem& default_instance();

  void Swap(Problem* other);

  // implements Message ----------------------------------------------

  Problem* New() const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const Problem& from);
  void MergeFrom(const Problem& from);
  void Clear();
  bool IsInitialized() const;

  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:

  ::google::protobuf::Metadata GetMetadata() const;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // optional .ProblemBuffers.ProblemSettings settings = 1;
  inline bool has_settings() const;
  inline void clear_settings();
  static const int kSettingsFieldNumber = 1;
  inline const ::ProblemBuffers::ProblemSettings& settings() const;
  inline ::ProblemBuffers::ProblemSettings* mutable_settings();
  inline ::ProblemBuffers::ProblemSettings* release_settings();
  inline void set_allocated_settings(::ProblemBuffers::ProblemSettings* settings);

  // repeated .RemyBuffers.NetConfig configs = 2;
  inline int configs_size() const;
  inline void clear_configs();
  static const int kConfigsFieldNumber = 2;
  inline const ::RemyBuffers::NetConfig& configs(int index) const;
  inline ::RemyBuffers::NetConfig* mutable_configs(int index);
  inline ::RemyBuffers::NetConfig* add_configs();
  inline const ::google::protobuf::RepeatedPtrField< ::RemyBuffers::NetConfig >&
      configs() const;
  inline ::google::protobuf::RepeatedPtrField< ::RemyBuffers::NetConfig >*
      mutable_configs();

  // optional .RemyBuffers.WhiskerTree whiskers = 3;
  inline bool has_whiskers() const;
  inline void clear_whiskers();
  static const int kWhiskersFieldNumber = 3;
  inline const ::RemyBuffers::WhiskerTree& whiskers() const;
  inline ::RemyBuffers::WhiskerTree* mutable_whiskers();
  inline ::RemyBuffers::WhiskerTree* release_whiskers();
  inline void set_allocated_whiskers(::RemyBuffers::WhiskerTree* whiskers);

  // @@protoc_insertion_point(class_scope:ProblemBuffers.Problem)
 private:
  inline void set_has_settings();
  inline void clear_has_settings();
  inline void set_has_whiskers();
  inline void clear_has_whiskers();

  ::google::protobuf::UnknownFieldSet _unknown_fields_;

  ::ProblemBuffers::ProblemSettings* settings_;
  ::google::protobuf::RepeatedPtrField< ::RemyBuffers::NetConfig > configs_;
  ::RemyBuffers::WhiskerTree* whiskers_;

  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(3 + 31) / 32];

  friend void  protobuf_AddDesc_problem_2eproto();
  friend void protobuf_AssignDesc_problem_2eproto();
  friend void protobuf_ShutdownFile_problem_2eproto();

  void InitAsDefaultInstance();
  static Problem* default_instance_;
};
// -------------------------------------------------------------------

class ProblemSettings : public ::google::protobuf::Message {
 public:
  ProblemSettings();
  virtual ~ProblemSettings();

  ProblemSettings(const ProblemSettings& from);

  inline ProblemSettings& operator=(const ProblemSettings& from) {
    CopyFrom(from);
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _unknown_fields_;
  }

  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return &_unknown_fields_;
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const ProblemSettings& default_instance();

  void Swap(ProblemSettings* other);

  // implements Message ----------------------------------------------

  ProblemSettings* New() const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const ProblemSettings& from);
  void MergeFrom(const ProblemSettings& from);
  void Clear();
  bool IsInitialized() const;

  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output) const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:

  ::google::protobuf::Metadata GetMetadata() const;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // optional uint32 prng_seed = 11;
  inline bool has_prng_seed() const;
  inline void clear_prng_seed();
  static const int kPrngSeedFieldNumber = 11;
  inline ::google::protobuf::uint32 prng_seed() const;
  inline void set_prng_seed(::google::protobuf::uint32 value);

  // optional uint32 tick_count = 12;
  inline bool has_tick_count() const;
  inline void clear_tick_count();
  static const int kTickCountFieldNumber = 12;
  inline ::google::protobuf::uint32 tick_count() const;
  inline void set_tick_count(::google::protobuf::uint32 value);

  // @@protoc_insertion_point(class_scope:ProblemBuffers.ProblemSettings)
 private:
  inline void set_has_prng_seed();
  inline void clear_has_prng_seed();
  inline void set_has_tick_count();
  inline void clear_has_tick_count();

  ::google::protobuf::UnknownFieldSet _unknown_fields_;

  ::google::protobuf::uint32 prng_seed_;
  ::google::protobuf::uint32 tick_count_;

  mutable int _cached_size_;
  ::google::protobuf::uint32 _has_bits_[(2 + 31) / 32];

  friend void  protobuf_AddDesc_problem_2eproto();
  friend void protobuf_AssignDesc_problem_2eproto();
  friend void protobuf_ShutdownFile_problem_2eproto();

  void InitAsDefaultInstance();
  static ProblemSettings* default_instance_;
};
// ===================================================================


// ===================================================================

// Problem

// optional .ProblemBuffers.ProblemSettings settings = 1;
inline bool Problem::has_settings() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void Problem::set_has_settings() {
  _has_bits_[0] |= 0x00000001u;
}
inline void Problem::clear_has_settings() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void Problem::clear_settings() {
  if (settings_ != NULL) settings_->::ProblemBuffers::ProblemSettings::Clear();
  clear_has_settings();
}
inline const ::ProblemBuffers::ProblemSettings& Problem::settings() const {
  return settings_ != NULL ? *settings_ : *default_instance_->settings_;
}
inline ::ProblemBuffers::ProblemSettings* Problem::mutable_settings() {
  set_has_settings();
  if (settings_ == NULL) settings_ = new ::ProblemBuffers::ProblemSettings;
  return settings_;
}
inline ::ProblemBuffers::ProblemSettings* Problem::release_settings() {
  clear_has_settings();
  ::ProblemBuffers::ProblemSettings* temp = settings_;
  settings_ = NULL;
  return temp;
}
inline void Problem::set_allocated_settings(::ProblemBuffers::ProblemSettings* settings) {
  delete settings_;
  settings_ = settings;
  if (settings) {
    set_has_settings();
  } else {
    clear_has_settings();
  }
}

// repeated .RemyBuffers.NetConfig configs = 2;
inline int Problem::configs_size() const {
  return configs_.size();
}
inline void Problem::clear_configs() {
  configs_.Clear();
}
inline const ::RemyBuffers::NetConfig& Problem::configs(int index) const {
  return configs_.Get(index);
}
inline ::RemyBuffers::NetConfig* Problem::mutable_configs(int index) {
  return configs_.Mutable(index);
}
inline ::RemyBuffers::NetConfig* Problem::add_configs() {
  return configs_.Add();
}
inline const ::google::protobuf::RepeatedPtrField< ::RemyBuffers::NetConfig >&
Problem::configs() const {
  return configs_;
}
inline ::google::protobuf::RepeatedPtrField< ::RemyBuffers::NetConfig >*
Problem::mutable_configs() {
  return &configs_;
}

// optional .RemyBuffers.WhiskerTree whiskers = 3;
inline bool Problem::has_whiskers() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void Problem::set_has_whiskers() {
  _has_bits_[0] |= 0x00000004u;
}
inline void Problem::clear_has_whiskers() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void Problem::clear_whiskers() {
  if (whiskers_ != NULL) whiskers_->::RemyBuffers::WhiskerTree::Clear();
  clear_has_whiskers();
}
inline const ::RemyBuffers::WhiskerTree& Problem::whiskers() const {
  return whiskers_ != NULL ? *whiskers_ : *default_instance_->whiskers_;
}
inline ::RemyBuffers::WhiskerTree* Problem::mutable_whiskers() {
  set_has_whiskers();
  if (whiskers_ == NULL) whiskers_ = new ::RemyBuffers::WhiskerTree;
  return whiskers_;
}
inline ::RemyBuffers::WhiskerTree* Problem::release_whiskers() {
  clear_has_whiskers();
  ::RemyBuffers::WhiskerTree* temp = whiskers_;
  whiskers_ = NULL;
  return temp;
}
inline void Problem::set_allocated_whiskers(::RemyBuffers::WhiskerTree* whiskers) {
  delete whiskers_;
  whiskers_ = whiskers;
  if (whiskers) {
    set_has_whiskers();
  } else {
    clear_has_whiskers();
  }
}

// -------------------------------------------------------------------

// ProblemSettings

// optional uint32 prng_seed = 11;
inline bool ProblemSettings::has_prng_seed() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void ProblemSettings::set_has_prng_seed() {
  _has_bits_[0] |= 0x00000001u;
}
inline void ProblemSettings::clear_has_prng_seed() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void ProblemSettings::clear_prng_seed() {
  prng_seed_ = 0u;
  clear_has_prng_seed();
}
inline ::google::protobuf::uint32 ProblemSettings::prng_seed() const {
  return prng_seed_;
}
inline void ProblemSettings::set_prng_seed(::google::protobuf::uint32 value) {
  set_has_prng_seed();
  prng_seed_ = value;
}

// optional uint32 tick_count = 12;
inline bool ProblemSettings::has_tick_count() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void ProblemSettings::set_has_tick_count() {
  _has_bits_[0] |= 0x00000002u;
}
inline void ProblemSettings::clear_has_tick_count() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void ProblemSettings::clear_tick_count() {
  tick_count_ = 0u;
  clear_has_tick_count();
}
inline ::google::protobuf::uint32 ProblemSettings::tick_count() const {
  return tick_count_;
}
inline void ProblemSettings::set_tick_count(::google::protobuf::uint32 value) {
  set_has_tick_count();
  tick_count_ = value;
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace ProblemBuffers

#ifndef SWIG
namespace google {
namespace protobuf {


}  // namespace google
}  // namespace protobuf
#endif  // SWIG

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_problem_2eproto__INCLUDED