#ifndef MIDI_H_INCLUDED
#define MIDI_H_INCLUDED

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <sstream>

namespace smf {

class MidiEvent;
class MidiMessage;
class MidiEventList;

class MidiFile {
public:
	MidiFile(void);
	MidiFile(const std::string& filename);
	MidiFile(std::istream& input);

	~MidiFile();

	int read(const std::string& filename);
	int read(std::istream& input);
	int write(const std::string& filename);
	int write(std::ostream& out);

	int getTrackCount(void) const;
	MidiEventList& operator[](int track);
	const MidiEventList& operator[](int track) const;

	void doTimeAnalysis(void);
	double getSecondsPerQuarterNote(int track, int index);
    void linkNotePairs(void);

protected:
	std::vector<MidiEventList*> m_events;
	int m_ticksPerQuarterNote;

private:
	int readSmf(std::istream& input);
	int extractMidiData(std::istream& input, std::vector<uint8>& array, uint8& runningCommand);
};

class MidiEvent : public std::vector<uint8> {
public:
	MidiEvent(void);
	MidiEvent(int command);
	MidiEvent(int command, int p1);
	MidiEvent(int command, int p1, int p2);
	MidiEvent(const MidiMessage& message);

	~MidiEvent();

	int tick;
	int track;
	double seconds;

	bool isNoteOn(void) const;
	bool isNoteOff(void) const;
	int getKeyNumber(void) const;
	int getVelocity(void) const;
	double getDurationInSeconds(void) const;
	void setDurationInSeconds(double duration);
    MidiEvent* getLinkedEvent(void);
    void setLinkedEvent(MidiEvent* event);

protected:
	double m_duration;
    MidiEvent* m_linkedEvent;
};

class MidiEventList {
public:
	MidiEventList(void);
	~MidiEventList();

	MidiEvent& operator[](int index);
	const MidiEvent& operator[](int index) const;
	int getSize(void) const;
	int size(void) const;
	void push_back(const MidiEvent& event);
	void linkNotePairs(void);

protected:
	std::vector<MidiEvent> m_list;
};

} // namespace smf

#endif // MIDI_H_INCLUDED

#ifdef MIDI_IMPLEMENTATION

namespace smf {

MidiFile::MidiFile(void) {
	m_ticksPerQuarterNote = 120;
}

MidiFile::MidiFile(const std::string& filename) {
	m_ticksPerQuarterNote = 120;
	read(filename);
}

MidiFile::MidiFile(std::istream& input) {
	m_ticksPerQuarterNote = 120;
	read(input);
}

MidiFile::~MidiFile() {
	for (int i = 0; i < getTrackCount(); i++) {
		delete m_events[i];
	}
}

int MidiFile::read(const std::string& filename) {
	std::ifstream input(filename.c_str(), std::ios::binary);
	if (!input) {
		return 0;
	}
	return read(input);
}

int MidiFile::read(std::istream& input) {
	return readSmf(input);
}

int MidiFile::write(const std::string& filename) {
	std::ofstream output(filename.c_str(), std::ios::binary);
	if (!output) {
		return 0;
	}
	return write(output);
}

int MidiFile::write(std::ostream& out) {
	// Simplified write function
	return 1;
}

int MidiFile::getTrackCount(void) const {
	return (int)m_events.size();
}

MidiEventList& MidiFile::operator[](int track) {
	return *m_events[track];
}

const MidiEventList& MidiFile::operator[](int track) const {
	return *m_events[track];
}

void MidiFile::doTimeAnalysis(void) {
	if (getTrackCount() == 0) {
		return;
	}
	double tempo = 120.0; // beats per minute
	for (int i = 0; i < getTrackCount(); i++) {
		double timesum = 0;
		for (int j = 0; j < (*this)[i].size(); j++) {
			int ticks = (*this)[i][j].tick;
			double seconds = (double)ticks / m_ticksPerQuarterNote * (60.0 / tempo);
			timesum += seconds;
			(*this)[i][j].seconds = timesum;

			// Check for tempo change
			if ((*this)[i][j].size() >= 6 && (*this)[i][j][0] == 0xFF && (*this)[i][j][1] == 0x51) {
				int microseconds = ((*this)[i][j][3] << 16) | ((*this)[i][j][4] << 8) | (*this)[i][j][5];
				tempo = 60000000.0 / microseconds;
			}
		}
	}
	for (int i = 0; i < getTrackCount(); i++) {
		(*this)[i].linkNotePairs();
	}
}

double MidiFile::getSecondsPerQuarterNote(int track, int index) {
	// Simplified
	return 60.0 / 120.0;
}

void MidiFile::linkNotePairs(void) {
    for (int i=0; i<getTrackCount(); i++) {
        (*this)[i].linkNotePairs();
    }
}

int MidiFile::readSmf(std::istream& input) {
	char buffer[1024] = { 0 };

	// Read MThd chunk
	input.read(buffer, 8);
	if (input.gcount() != 8) return 0;
	if (strncmp(buffer, "MThd", 4) != 0) return 0;
	int length = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
	if (length != 6) return 0;

	input.read(buffer, 6);
	if (input.gcount() != 6) return 0;
	int numtracks = (buffer[2] << 8) | buffer[3];
	m_ticksPerQuarterNote = (buffer[4] << 8) | buffer[5];

	for (int i = 0; i < numtracks; i++) {
		m_events.push_back(new MidiEventList);
		uint8 runningCommand = 0;
		input.read(buffer, 8);
		if (input.gcount() != 8) return 0;
		if (strncmp(buffer, "MTrk", 4) != 0) return 0;
		int tracklength = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];

		std::vector<uint8> trackdata;
		trackdata.resize(tracklength);
		input.read((char*)trackdata.data(), tracklength);
		if (input.gcount() != tracklength) return 0;

		int current_tick = 0;
		int idx = 0;
		while (idx < tracklength) {
			int deltatick = 0;
			uint8 byte;
			do {
				byte = trackdata[idx++];
				deltatick = (deltatick << 7) | (byte & 0x7F);
			} while (byte & 0x80);
			current_tick += deltatick;

			MidiEvent event;
			event.tick = current_tick;
			event.track = i;

			if (trackdata[idx] >= 0x80) {
				runningCommand = trackdata[idx++];
			}

			event.push_back(runningCommand);
			switch (runningCommand & 0xF0) {
			case 0x80: // note off
			case 0x90: // note on
			case 0xA0: // aftertouch
			case 0xB0: // continuous controller
			case 0xE0: // pitch wheel
				event.push_back(trackdata[idx++]);
				event.push_back(trackdata[idx++]);
				break;
			case 0xC0: // patch change
			case 0xD0: // channel pressure
				event.push_back(trackdata[idx++]);
				break;
			case 0xF0: // sysex and meta events
				if (runningCommand == 0xFF) { // meta event
					event.push_back(trackdata[idx++]); // type
					int metalength = 0;
					do {
						byte = trackdata[idx++];
						metalength = (metalength << 7) | (byte & 0x7F);
					} while (byte & 0x80);
					for (int k = 0; k < metalength; k++) {
						event.push_back(trackdata[idx++]);
					}
				}
				// sysex not fully supported in this simplified version
				break;
			}
			(*m_events.back()).push_back(event);
		}
	}
	return 1;
}


MidiEvent::MidiEvent(void) { tick = 0; track = 0; seconds = 0; m_duration = 0; m_linkedEvent = NULL; }
MidiEvent::MidiEvent(int command) : std::vector<uint8>(1, (uint8)command) { tick = 0; track = 0; seconds = 0; m_duration = 0; m_linkedEvent = NULL; }
MidiEvent::MidiEvent(int command, int p1) : std::vector<uint8>(2) { (*this)[0] = (uint8)command; (*this)[1] = (uint8)p1; tick = 0; track = 0; seconds = 0; m_duration = 0; m_linkedEvent = NULL; }
MidiEvent::MidiEvent(int command, int p1, int p2) : std::vector<uint8>(3) { (*this)[0] = (uint8)command; (*this)[1] = (uint8)p1; (*this)[2] = (uint8)p2; tick = 0; track = 0; seconds = 0; m_duration = 0; m_linkedEvent = NULL; }
MidiEvent::MidiEvent(const MidiMessage& message) : std::vector<uint8>(message) { tick = 0; track = 0; seconds = 0; m_duration = 0; m_linkedEvent = NULL; }
MidiEvent::~MidiEvent() {}
bool MidiEvent::isNoteOn(void) const { return ((*this)[0] & 0xF0) == 0x90 && (*this)[2] > 0; }
bool MidiEvent::isNoteOff(void) const { return ((*this)[0] & 0xF0) == 0x80 || ((*this)[0] & 0xF0) == 0x90 && (*this)[2] == 0; }
int MidiEvent::getKeyNumber(void) const { return (*this)[1]; }
int MidiEvent::getVelocity(void) const { return (*this)[2]; }
double MidiEvent::getDurationInSeconds(void) const { return m_duration; }
void MidiEvent::setDurationInSeconds(double duration) { m_duration = duration; }
MidiEvent* MidiEvent::getLinkedEvent(void) { return m_linkedEvent; }
void MidiEvent::setLinkedEvent(MidiEvent* event) { m_linkedEvent = event; }


MidiEventList::MidiEventList(void) {}
MidiEventList::~MidiEventList() {}
MidiEvent& MidiEventList::operator[](int index) { return m_list[index]; }
const MidiEvent& MidiEventList::operator[](int index) const { return m_list[index]; }
int MidiEventList::getSize(void) const { return (int)m_list.size(); }
int MidiEventList::size(void) const { return (int)m_list.size(); }
void MidiEventList::push_back(const MidiEvent& event) { m_list.push_back(event); }
void MidiEventList::linkNotePairs(void) {
	std::vector<MidiEvent*> noteons;
	noteons.reserve(getSize());
	for (int i = 0; i < getSize(); i++) {
		if ((*this)[i].isNoteOn()) {
			noteons.push_back(&(*this)[i]);
		}
	}

	for (int i = 0; i < (int)noteons.size(); i++) {
		int key = (*noteons[i])[1];
		for (int j = 0; j < getSize(); j++) {
			if ((*this)[j].isNoteOff() && (*this)[j][1] == key && (*this)[j].tick > noteons[i]->tick) {
				noteons[i]->setDurationInSeconds((*this)[j].seconds - noteons[i]->seconds);
                noteons[i]->setLinkedEvent(&m_list[j]);
				break;
			}
		}
	}
}

} // namespace smf

#endif // MIDI_IMPLEMENTATION

