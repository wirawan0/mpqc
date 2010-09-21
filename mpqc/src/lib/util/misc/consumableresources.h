//
// consumableresources.h
//
// Copyright (C) 2010 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
// Maintainer: EV
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifdef __GNUG__
#pragma interface
#endif

#ifndef _mpqc_src_lib_util_misc_consumableresources_h
#define _mpqc_src_lib_util_misc_consumableresources_h

#include <util/keyval/keyval.h>
#include <util/state/statein.h>
#include <util/state/stateout.h>

namespace sc {

  /// ConsumableResources keeps track of consumable resources (memory, disk).
  class ConsumableResources : virtual public SavableState {
    public:
      /** A KeyVal constructor is used to generate a ConsumableResources
          object from the input. The full list of keywords
          that are accepted is below.

          <table border="1">

          <tr><td><b>%Keyword</b><td><b>Type</b><td><b>Default</b><td><b>Description</b>

          <tr><td><tt>memory</tt><td>integer<td>256000000<td>number of bytes; user is allowed to use KB/MB/GB abbreviations

          <tr><td><tt>disk</tt><td>[string integer] pair<td>["/tmp/" 0]<td>specifies location of scratch files and available storage in bytes ("0" means unlimited)

          </table>
       */
      ConsumableResources(const Ref<KeyVal>& kv);
      ConsumableResources(StateIn&);
      ConsumableResources();
      ~ConsumableResources();
      void save_data_state(StateOut&);

      //@{
      /// how much resource was given
      size_t max_memory() const;
      size_t max_disk() const;
      //@}

      //@{
      /// how much resource is currently available
      size_t memory() const;
      size_t disk() const;
      //@}

      //@{
      /// consume resource, may throw LimitExceeded<size_t> if not enough available
      void consume_memory(size_t value);
      void consume_disk(size_t value);
      //@}

      //@{
      /// release resouce, may throw ProgrammingError if releasing more resource than how much has been consumed to this point
      void release_memory(size_t value);
      void release_disk(size_t value);
      //@}

      /// UNIX path (absolute or relative) to the disk resource
      const std::string& disk_location() const;

      /** Create a ConsumableResources object.  This routine looks for a -resource
          argument, then the environmental variable SC_RESOURCES.
          The argument to -resources should be a string for the KeyVal constructor. */
      static Ref<ConsumableResources> initial_instance(int &argc, char **argv);
      /// Specifies a new default ConsumableResources
      static void set_default_instance(const Ref<ConsumableResources>&);
      /// Returns the default ConsumableResources object
      static const Ref<ConsumableResources>& get_default_instance();

      /// prints definition
      std::string print() const;

    private:
      static ClassDesc class_desc_;

      struct defaults {
          static size_t memory;
          static std::pair<std::string,size_t> disk;
      };

      template <typename T> class ResourceCounter {
        public:
          ResourceCounter(const T& max_value = T()) : max_value_(max_value), value_(max_value) {}
          ResourceCounter(const T& max_value, const T& value) : max_value_(max_value), value_(value) {}
          ResourceCounter(const ResourceCounter& other) : max_value_(other.max_value), value_(other.value) {}
          ResourceCounter& operator=(const ResourceCounter& other) { max_value_ = other.max_value_; value_ = other.value_; return *this; }
          operator T() const { return value_; }
          ResourceCounter& operator+=(const T& val) { value_ = std::max(max_value_, value_ + val); return *this; }
          // nonthrowing
          ResourceCounter& operator-=(const T& val) { value_ = std::min(T(0), value_ - val); return *this; }

          const T& max_value() const { return max_value_; }
          const T& value() const { return value_; }

          void operator &(StateIn& s) {
            s.get(max_value_);
            s.get(value_);
          }
          void operator &(StateOut& s) {
            s.put(max_value_);
            s.put(value_);
          }

        private:
          T max_value_;
          T value_;
      };

      typedef ResourceCounter<size_t> rsize;
      rsize memory_;
      std::pair<std::string, rsize> disk_;

      static Ref<ConsumableResources> default_instance_;

  };


} // end of namespace sc

#endif // end of header guard


// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End: