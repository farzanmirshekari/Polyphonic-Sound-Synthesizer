#include <iostream>
#include <cmath>
#include <algorithm>
#include <list>
using namespace std;
#include "noiseMaker.h"

namespace synthesizer {

	double w(double dHertz) {
		return dHertz * 2.0 * PI;
	}

	struct note {
		int id;
		double on;
		double off;
		bool active;
		int channel;

		note() {
			id = 0.5;
			on = 0.0;
			off = 0.0;
			active = false;
			channel = 0;
		}
	};

#define OSC_SINE 0
#define OSC_SQUARE 1
#define OSC_TRIANGLE 2
#define OSC_SAW_ANALOGUE 3
#define OSC_SAW_DIGITAL 4
#define OSC_NOISE 5
	// general purpose oscillator
	double osc(const double dTime, const double dHertz, const int nType = OSC_SINE,
		const double dLFOHertz = 0.0, const double dLFOAmplitude = 0.0, double dCustom = 50.0) {

		double dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * (sin(w(dLFOHertz) * dTime));

		switch (nType)
		{
		case OSC_SINE:
			return sin(dFreq);

		case OSC_SQUARE:
			return sin(dFreq) > 0 ? 1.0 : -1.0;

		case OSC_TRIANGLE:
			return asin(sin(dFreq)) * (2.0 / PI);

		case OSC_SAW_ANALOGUE: {
			double dOutput = 0.0;
			for (double n = 1.0; n < dCustom; n++)
				dOutput += (sin(n * dFreq)) / n;
			return dOutput * (2.0 / PI);
		}
		case OSC_SAW_DIGITAL:
			return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));

		case OSC_NOISE:
			return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;

		default:
			return 0.0;
		}
	}

	const double SCALE_BASE = pow(2.0, (1.0 / 12.0));
	const int SCALE_DEFAULT = 0;

	double scale(const int nNoteID, const int nScaleID = SCALE_DEFAULT) {
		switch (nScaleID) {
		case SCALE_DEFAULT: default:
			return 256 * pow(SCALE_BASE, nNoteID);
		}
	}

	struct envelope {
		virtual double amplitude(const double dTime, const double dTimeOn, const double dTimeOff) = 0;
	};

	struct envelope_adsr : public envelope {

		double dAttackTime;
		double dDecayTime;
		double dSustainAmplitude;
		double dReleaseTime;
		double dStartAmplitude;

		envelope_adsr()
		{
			dAttackTime = 0.1;
			dDecayTime = 0.1;
			dSustainAmplitude = 1.0;
			dReleaseTime = 0.2;
			dStartAmplitude = 1.0;
		}

		virtual double amplitude(const double dTime, const double dTimeOn, const double dTimeOff)
		{
			double dAmplitude = 0.0;
			double dReleaseAmplitude = 0.0;

			if (dTimeOn > dTimeOff) // note is on
			{
				double dLifeTime = dTime - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dAmplitude = dSustainAmplitude;
			}
			else // note is off
			{
				double dLifeTime = dTimeOff - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dReleaseAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dReleaseAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dReleaseAmplitude = dSustainAmplitude;

				dAmplitude = ((dTime - dTimeOff) / dReleaseTime) * (0.0 - dReleaseAmplitude) + dReleaseAmplitude;
			}

			// amplitude should not be negative
			if (dAmplitude <= 0.000)
				dAmplitude = 0.0;

			return dAmplitude;
		}
	};

	double env(const double dTime, envelope& env, const double dTimeOn, const double dTimeOff) {
		return env.amplitude(dTime, dTimeOn, dTimeOff);
	}


	struct instrument {
		double dVolume;
		synthesizer::envelope_adsr env;
		virtual double sound(const double dTime, synthesizer::note n, bool& bNoteFinished) = 0;
	};

	struct bell : public instrument {

		bell()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 1.0;
			dVolume = 1.0;
		}

		virtual double sound(const double dTime, synthesizer::note n, bool& bNoteFinished) {
			double dAmplitude = synthesizer::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			double dSound =
				+1.00 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 12), OSC_SINE, 5.0, 0.001)
				+ 0.50 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 24))
				+ 0.25 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 36));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct harmonica : public instrument {

		harmonica() {
			env.dAttackTime = 0.05;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.95;
			env.dReleaseTime = 0.1;
			dVolume = 1.0;
		}

		virtual double sound(const double dTime, synthesizer::note n, bool& bNoteFinished)
		{
			double dAmplitude = synthesizer::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			double dSound = (
				+1.0 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id - 3), OSC_SAW_ANALOGUE, 5.0, 0.001, 100)
				+ 1.0 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 9), OSC_SQUARE, 5.0, 0.001)
				+ 0.5 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 21), OSC_SQUARE)
				+ 0.05 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 33), OSC_NOISE)
				);

			return dAmplitude * dSound * dVolume;
		}

	};

	struct piano : public instrument {

		piano() {
			env.dAttackTime = 0.005;
			env.dDecayTime = 0.2;
			env.dSustainAmplitude = 0.6;
			env.dReleaseTime = 0.8;
			dVolume = 1.0;
		}

		virtual double sound(const double dTime, synthesizer::note n, bool& bNoteFinished)
		{
			double dAmplitude = synthesizer::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;
			// more work to be done here
			double dSound = (
				+1.0 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 9), OSC_SINE)
				+ 0.05 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 21), OSC_SQUARE)
				+ 1.5 * synthesizer::osc(n.on - dTime, synthesizer::scale(n.id + 33), OSC_TRIANGLE)
				);

			return dAmplitude * dSound * dVolume;
		}

	};

}

// global variables
vector<synthesizer::note> vecNotes;
mutex muxNotes;
synthesizer::harmonica harmonica;
synthesizer::bell bell;
synthesizer::piano piano;


typedef bool(*lambda)(synthesizer::note const& item);
template<class T>
void safe_remove(T& v, lambda f)
{
	auto n = v.begin();
	while (n != v.end())
		if (!f(*n))
			n = v.erase(n);
		else
			++n;
}


double makeNoise(int nChannel, double dTime) {
	unique_lock<mutex> lm(muxNotes);
	double dMixedOutput = 0.0;

	for (auto& n : vecNotes)
	{
		bool bNoteFinished = false;
		double dSound = 0;
		if (n.channel == 1)
			dSound = harmonica.sound(dTime, n, bNoteFinished) * 0.5;
		if (n.channel == 2)
			dSound = bell.sound(dTime, n, bNoteFinished);
		if (n.channel == 3)
			dSound = piano.sound(dTime, n, bNoteFinished);
		dMixedOutput += dSound;

		if (bNoteFinished && n.off > n.on)
			n.active = false;
	}

	safe_remove<vector<synthesizer::note>>(vecNotes, [](synthesizer::note const& item) { return item.active; });


	return dMixedOutput * 0.2;

}


int main()
{
	wcout << "Farzan's Synthesizer" << endl;

	vector<wstring> devices = noiseMaker<short>::Enumerate();

	for (auto d : devices) wcout << "Found Output Device: " << d << endl;

	wcout << "Using Device: " << devices[0] << endl;

	wcout << endl <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << endl <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << endl <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << endl <<
		"|     |     |     |     |     |     |     |     |     |     |" << endl <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << endl <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << endl;

	noiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

	sound.SetUserFunction(makeNoise);

	char keyboard[129];
	memset(keyboard, ' ', 127);
	keyboard[128] = '\0';

	auto clock_old_time = chrono::high_resolution_clock::now();
	auto clock_real_time = chrono::high_resolution_clock::now();
	double dElapsedTime = 0.0;

	while (1)
	{
		for (int k = 0; k < 16; k++)
		{
			short nKeyState = GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k]));

			double dTimeNow = sound.GetTime();

			muxNotes.lock();
			auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synthesizer::note const& item) { return item.id == k; });
			if (noteFound == vecNotes.end()) {
				// note not in vector, must be added
				if (nKeyState & 0x8000) {
					synthesizer::note n;
					n.id = k;
					n.on = dTimeNow;
					n.channel = 1;
					n.active = true;
					vecNotes.emplace_back(n);
				}
			}
			else {
				// note in vector
				if (nKeyState & 0x8000)
				{
					// key is still held
					if (noteFound->off > noteFound->on) {
						noteFound->on = dTimeNow;
						noteFound->active = true;
					}
				}
				else {
					if (noteFound->off < noteFound->on)
					{
						noteFound->off = dTimeNow;
					}
				}
			}
			muxNotes.unlock();
		}
	}


	return 0;
}