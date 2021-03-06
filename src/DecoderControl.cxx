/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "DecoderControl.hxx"
#include "MusicPipe.hxx"
#include "Song.hxx"

#include <glib.h>

#include <assert.h>

decoder_control::decoder_control()
	:thread(nullptr),
	 state(DecoderState::STOP),
	 command(DecoderCommand::NONE),
	 song(nullptr),
	 replay_gain_db(0), replay_gain_prev_db(0),
	 mixramp_start(nullptr), mixramp_end(nullptr),
	 mixramp_prev_end(nullptr) {}

decoder_control::~decoder_control()
{
	ClearError();

	if (song != NULL)
		song->Free();

	g_free(mixramp_start);
	g_free(mixramp_end);
	g_free(mixramp_prev_end);
}

bool
decoder_control::IsCurrentSong(const Song *_song) const
{
	assert(_song != NULL);

	switch (state) {
	case DecoderState::STOP:
	case DecoderState::ERROR:
		return false;

	case DecoderState::START:
	case DecoderState::DECODE:
		return song_equals(song, _song);
	}

	assert(false);
	gcc_unreachable();
}

void
decoder_control::Start(Song *_song,
		       unsigned _start_ms, unsigned _end_ms,
		       MusicBuffer &_buffer, MusicPipe &_pipe)
{
	assert(_song != NULL);
	assert(_pipe.IsEmpty());

	if (song != nullptr)
		song->Free();

	song = _song;
	start_ms = _start_ms;
	end_ms = _end_ms;
	buffer = &_buffer;
	pipe = &_pipe;

	LockSynchronousCommand(DecoderCommand::START);
}

void
decoder_control::Stop()
{
	Lock();

	if (command != DecoderCommand::NONE)
		/* Attempt to cancel the current command.  If it's too
		   late and the decoder thread is already executing
		   the old command, we'll call STOP again in this
		   function (see below). */
		SynchronousCommandLocked(DecoderCommand::STOP);

	if (state != DecoderState::STOP && state != DecoderState::ERROR)
		SynchronousCommandLocked(DecoderCommand::STOP);

	Unlock();
}

bool
decoder_control::Seek(double where)
{
	assert(state != DecoderState::START);
	assert(where >= 0.0);

	if (state == DecoderState::STOP ||
	    state == DecoderState::ERROR || !seekable)
		return false;

	seek_where = where;
	seek_error = false;
	LockSynchronousCommand(DecoderCommand::SEEK);

	return !seek_error;
}

void
decoder_control::Quit()
{
	assert(thread != nullptr);

	quit = true;
	LockAsynchronousCommand(DecoderCommand::STOP);

	g_thread_join(thread);
	thread = nullptr;
}

void
decoder_control::MixRampStart(char *_mixramp_start)
{
	g_free(mixramp_start);
	mixramp_start = _mixramp_start;
}

void
decoder_control::MixRampEnd(char *_mixramp_end)
{
	g_free(mixramp_end);
	mixramp_end = _mixramp_end;
}

void
decoder_control::MixRampPrevEnd(char *_mixramp_prev_end)
{
	g_free(mixramp_prev_end);
	mixramp_prev_end = _mixramp_prev_end;
}
