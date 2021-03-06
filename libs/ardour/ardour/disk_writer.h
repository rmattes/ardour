/*
    Copyright (C) 2009-2016 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_disk_writer_h__
#define __ardour_disk_writer_h__

#include <list>
#include <vector>

#include "pbd/i18n.h"

#include "ardour/disk_io.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR
{

class AudioFileSource;
class SMFSource;
class MidiSource;

class LIBARDOUR_API DiskWriter : public DiskIOProcessor
{
  public:
	DiskWriter (Session&, std::string const & name, DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));
	~DiskWriter ();

	bool set_name (std::string const & str);
	std::string display_name() const { return std::string (_("recorder")); }

	virtual bool set_write_source_name (const std::string& str);

	bool           recordable()  const { return _flags & Recordable; }

	static samplecnt_t chunk_samples() { return _chunk_samples; }
	static samplecnt_t default_chunk_samples ();
	static void set_chunk_samples (samplecnt_t n) { _chunk_samples = n; }

	void run (BufferSet& /*bufs*/, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void non_realtime_locate (samplepos_t);
	void realtime_handle_transport_stopped ();

	virtual XMLNode& state (bool full);
	int set_state (const XMLNode&, int version);

	std::string write_source_name () const {
		if (_write_source_name.empty()) {
			return name();
		} else {
			return _write_source_name;
		}
	}

	boost::shared_ptr<AudioFileSource> audio_write_source (uint32_t n=0) {
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (n < c->size()) {
			return (*c)[n]->write_source;
		}

		return boost::shared_ptr<AudioFileSource>();
	}

	boost::shared_ptr<SMFSource> midi_write_source () { return _midi_write_source; }

	virtual std::string steal_write_source_name ();
	int use_new_write_source (DataType, uint32_t n = 0);
	void reset_write_sources (bool, bool force = false);

	AlignStyle  alignment_style() const { return _alignment_style; }
	AlignChoice alignment_choice() const { return _alignment_choice; }
	void       set_align_style (AlignStyle, bool force=false);
	void       set_align_choice (AlignChoice a, bool force=false);

	PBD::Signal0<void> AlignmentStyleChanged;

	void set_input_latency (samplecnt_t);

	bool configure_io (ChanCount in, ChanCount out);

	std::list<boost::shared_ptr<Source> >& last_capture_sources () { return _last_capture_sources; }

	bool         record_enabled() const { return g_atomic_int_get (const_cast<gint*>(&_record_enabled)); }
	bool         record_safe () const { return g_atomic_int_get (const_cast<gint*>(&_record_safe)); }
	virtual void set_record_enabled (bool yn);
	virtual void set_record_safe (bool yn);

	bool destructive() const { return _flags & Destructive; }
	int set_destructive (bool yn);
	int set_non_layered (bool yn);
	bool can_become_destructive (bool& requires_bounce) const;

	/** @return Start position of currently-running capture (in session samples) */
	samplepos_t current_capture_start() const { return capture_start_sample; }
	samplepos_t current_capture_end()   const { return capture_start_sample + capture_captured; }
	samplepos_t get_capture_start_sample (uint32_t n = 0) const;
	samplecnt_t get_captured_samples (uint32_t n = 0) const;

	float buffer_load() const;

	virtual void request_input_monitoring (bool) {}
	virtual void ensure_input_monitoring (bool) {}

	samplecnt_t   capture_offset() const { return _capture_offset; }
	virtual void set_capture_offset ();

	int seek (samplepos_t sample, bool complete_refill);

	static PBD::Signal0<void> Overrun;

	void set_note_mode (NoteMode m);

	/** Emitted when some MIDI data has been received for recording.
	 *  Parameter is the source that it is destined for.
	 *  A caller can get a copy of the data with get_gui_feed_buffer ()
	 */
	PBD::Signal1<void, boost::weak_ptr<MidiSource> > DataRecorded;

	PBD::Signal0<void> RecordEnableChanged;
	PBD::Signal0<void> RecordSafeChanged;

	void check_record_status (samplepos_t transport_sample, bool can_record);

	void transport_looped (samplepos_t transport_sample);
	void transport_stopped_wallclock (struct tm&, time_t, bool abort);

	void adjust_buffering ();

  protected:
	friend class Track;
	int do_flush (RunContext context, bool force = false);

	void get_input_sources ();
	void prepare_record_status (samplepos_t /*capture_start_sample*/);
	void set_align_style_from_io();
	void setup_destructive_playlist ();
	void use_destructive_playlist ();
	void prepare_to_stop (samplepos_t transport_pos, samplepos_t audible_sample);

	void engage_record_enable ();
	void disengage_record_enable ();
	void engage_record_safe ();
	void disengage_record_safe ();

	bool prep_record_enable ();
	bool prep_record_disable ();

	void calculate_record_range (
		Evoral::OverlapType ot, samplepos_t transport_sample, samplecnt_t nframes,
		samplecnt_t& rec_nframes, samplecnt_t& rec_offset
		);

	mutable Glib::Threads::Mutex capture_info_lock;
	CaptureInfos capture_info;

  private:
	gint         _record_enabled;
	gint         _record_safe;
	samplepos_t    capture_start_sample;
	samplecnt_t    capture_captured;
	bool          was_recording;
	samplecnt_t    adjust_capture_position;
	samplecnt_t   _capture_offset;
	samplepos_t    first_recordable_sample;
	samplepos_t    last_recordable_sample;
	int           last_possibly_recording;
	AlignStyle   _alignment_style;
	AlignChoice  _alignment_choice;
	std::string   _write_source_name;
	boost::shared_ptr<SMFSource> _midi_write_source;

	std::list<boost::shared_ptr<Source> > _last_capture_sources;
	std::vector<boost::shared_ptr<AudioFileSource> > capturing_sources;

	static samplecnt_t _chunk_samples;

	NoteMode                     _note_mode;
	volatile gint                _samples_pending_write;
	volatile gint                _num_captured_loops;
	samplepos_t                   _accumulated_capture_offset;

	/** A buffer that we use to put newly-arrived MIDI data in for
	    the GUI to read (so that it can update itself).
	*/
	MidiBuffer                   _gui_feed_buffer;
	mutable Glib::Threads::Mutex _gui_feed_buffer_mutex;

	void finish_capture (boost::shared_ptr<ChannelList> c);
};

} // namespace

#endif /* __ardour_disk_writer_h__ */
