#include <bfsar/BankFile.h>

#include <ui/UI.h>

enum ImGuiPianoKeyboardMsg {
	NoteGetStatus,
	NoteOn,
	NoteOff,
};

using ImGuiPianoKeyboardProc = bool (*)(void* UserData, int Msg, int Key, float Vel);

struct ImGuiPianoStyles {
	ImU32 Colors[5]{
		IM_COL32(255, 255, 255, 255),	// light note
		IM_COL32(0, 0, 0, 255),			// dark note
		IM_COL32(255, 255, 0, 255),		// active light note
		IM_COL32(200, 200, 0, 255),		// active dark note
		IM_COL32(75, 75, 75, 255),		// background
	};
	float NoteDarkHeight = 2.0f / 3.0f; // dark note scale h
	float NoteDarkWidth  = 2.0f / 3.0f;	// dark note scale w
};

void ImGui_PianoKeyboard(const char* IDName, ImVec2 Size, int* PrevNoteActive, int BeginOctaveNote, int EndOctaveNote, ImGuiPianoKeyboardProc Callback, void* UserData, ImGuiPianoStyles* Style = nullptr);

bool KeyPresed[128] = { false };

bool TestPianoBoardFunct(void* UserData, int Msg, int Key, float Vel)
{
	if (!sSelectedItem || sSelectedItem->getItemType() != Item::ItemType::BankFileInstrument)
	{
		return false;
	}

    BankFile::Instrument* instrument = static_cast<BankFile::Instrument*>(sSelectedItem);

    if (Key < 0 || Key >= 128)
    {
        return false; // midi max keys
    }

    if (Msg == NoteGetStatus)
    {
        return KeyPresed[Key];
    }
    else if (Msg == NoteOn)
    {
        KeyPresed[Key] = true;
        //Send_Midi_NoteOn(Key, Vel*127);

        u8 vel = Vel * 127;

        const BankFile::KeyRegion* keyRegion = instrument->getKeyRegion(Key);
        if (!keyRegion)
        {
            return false;
        }

        const BankFile::VelocityRegion* velocityRegion = keyRegion->getVelocityRegion(vel);
        SEAD_ASSERT(velocityRegion);

		PlayBankNote(Key, vel, *velocityRegion);
    }
    else if (Msg == NoteOff)
    {
        KeyPresed[Key] = false;
        //Send_Midi_NoteOff(Key, Vel*127);

		StopAllSoundPlayers();
    }

    return false;
}

void BankFile::VelocityRegion::read(const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable)
{
    const nw::snd::internal::Util::WaveId* waveId = waveIdTable.GetWaveId(velocityRegionInfo->waveIdTableIndex);
    SEAD_ASSERT(waveId);

	//? waveId->waveIndex is patched with global wave index already
    WaveFile* waveFile = static_cast<WaveFile*>(sBfsar.getItem(waveId->waveIndex, sBfsar.getWaveFileList()));
    mWaveFileRef.attach(waveFile);

	mOriginalKey = velocityRegionInfo->GetOriginalKey();
	mVolume = velocityRegionInfo->GetVolume();
	mPan = velocityRegionInfo->GetPan();
	mPitch = velocityRegionInfo->GetPitch();
	mIsIgnoreNoteOff = velocityRegionInfo->IsIgnoreNoteOff();
	mKeyGroup = velocityRegionInfo->GetKeyGroup();
	mInterpolationType = velocityRegionInfo->GetInterpolationType();

	const nw::snd::AdshrCurve& adshrCurveInfo = velocityRegionInfo->GetAdshrCurve();
	mAdshrCurve.attack = adshrCurveInfo.attack;
	mAdshrCurve.decay = adshrCurveInfo.decay;
	mAdshrCurve.sustain = adshrCurveInfo.sustain;
	mAdshrCurve.hold = adshrCurveInfo.hold;
	mAdshrCurve.release = adshrCurveInfo.release;
}

void BankFile::KeyRegion::read(const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable)
{
	SEAD_ASSERT(keyRegionInfo);

	switch (nw::snd::internal::GetRegionType(keyRegionInfo->toVelocityRegionChunk.typeId))
	{
		case nw::snd::internal::REGION_TYPE_DIRECT:
		{
			VelocityRegion* velocityRegion = new VelocityRegion(0, 127);
			velocityRegion->mId = 0;

			velocityRegion->mEnableName = true;
			velocityRegion->mName = "VelocityRegion";

			velocityRegion->read(keyRegionInfo->GetVelocityRegion(0), waveIdTable);

			mVelocityRegionList.pushBack(velocityRegion);
			break;
		}

		case nw::snd::internal::REGION_TYPE_RANGE:
		{
			const nw::snd::internal::RangeChunk& rangeChunk = keyRegionInfo->GetRangeChunk();

			u32 velocityRegionCount = rangeChunk.borderTable.count;

			u32 velocityMin = 0;
			for (u32 i = 0; i < velocityRegionCount; i++)
			{
				const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo = static_cast<const nw::snd::internal::BankFile::VelocityRegion*>(rangeChunk.GetRegion(velocityMin));
				SEAD_ASSERT(velocityRegionInfo);

				u8 velocityMax = rangeChunk.borderTable.item[i];

				VelocityRegion* velocityRegion = new VelocityRegion(velocityMin, velocityMax);
				velocityRegion->mId = mVelocityRegionList.size();

				velocityRegion->mEnableName = true;
				velocityRegion->mName = "VelocityRegion";

				velocityRegion->read(velocityRegionInfo, waveIdTable);

				mVelocityRegionList.pushBack(velocityRegion);

				velocityMin = velocityMax + 1;
			}

			// SEAD_PRINT("KeyRegion(%u)\n", mId);
			// for (const Item* item : mVelocityRegionList)
			// {
			//     const VelocityRegion* velocityRegion = static_cast<const VelocityRegion*>(item);

			//     SEAD_PRINT(" - VelocityRegion(%u)\n", velocityRegion->mId);
			//     SEAD_PRINT("   - KeyMin: %u\n", velocityRegion->mKeyMin);
			//     SEAD_PRINT("   - KeyMax: %u\n\n", velocityRegion->mKeyMax);
			// }
			break;
		}

		case nw::snd::internal::REGION_TYPE_INDEX:
		{
			SEAD_ASSERT(false);
			break;
		}

		default:
			SEAD_ASSERT(false);
			break;
	}

	SEAD_ASSERT(mVelocityRegionList.size() > 0);
}

void BankFile::Instrument::read(const nw::snd::internal::BankFile::Instrument* instrumentInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable)
{
	SEAD_ASSERT(instrumentInfo);

	switch (nw::snd::internal::GetRegionType(instrumentInfo->toKeyRegionChunk.typeId))
	{
		case nw::snd::internal::REGION_TYPE_DIRECT:
		{
			KeyRegion* keyRegion = new KeyRegion(0, 127);
			keyRegion->mId = 0;

			keyRegion->mEnableName = true;
			keyRegion->mName = "KeyRegion";

			keyRegion->read(instrumentInfo->GetKeyRegion(0), waveIdTable);

			mKeyRegionList.pushBack(keyRegion);
			break;
		}

		case nw::snd::internal::REGION_TYPE_RANGE:
		{
			const nw::snd::internal::RangeChunk& rangeChunk = instrumentInfo->GetRangeChunk();

			u32 keyRegionCount = rangeChunk.borderTable.count;

			u32 keyMin = 0;
			for (u32 i = 0; i < keyRegionCount; i++)
			{
				const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo = static_cast<const nw::snd::internal::BankFile::KeyRegion*>(rangeChunk.GetRegion(keyMin));
				SEAD_ASSERT(keyRegionInfo);

				u8 keyMax = rangeChunk.borderTable.item[i];

				if (nw::snd::internal::GetRegionType(keyRegionInfo->toVelocityRegionChunk.typeId) == nw::snd::internal::REGION_TYPE_UNKNOWN)
				{
					keyMin = keyMax + 1;
					continue;
				}

				KeyRegion* keyRegion = new KeyRegion(keyMin, keyMax);
				keyRegion->mId = mKeyRegionList.size();

				keyRegion->mEnableName = true;
				keyRegion->mName = "KeyRegion";

				keyRegion->read(keyRegionInfo, waveIdTable);

				mKeyRegionList.pushBack(keyRegion);

				keyMin = keyMax + 1;
			}

			// SEAD_PRINT("Instrument(%u)\n", mId);
			// for (const Item* item : mKeyRegionList)
			// {
			//     const KeyRegion* keyRegion = static_cast<const KeyRegion*>(item);

			//     SEAD_PRINT(" - KeyRegion(%u)\n", keyRegion->mId);
			//     SEAD_PRINT("   - KeyMin: %u\n", keyRegion->mKeyMin);
			//     SEAD_PRINT("   - KeyMax: %u\n\n", keyRegion->mKeyMax);
			// }
			break;
		}

		case nw::snd::internal::REGION_TYPE_INDEX:
		{
			//SEAD_ASSERT(false);
			break;
		}

		default:
			SEAD_ASSERT(false);
			break;
	}

	//SEAD_ASSERT(mKeyRegionList.size() > 0);
}

void BankFile::Instrument::drawUI()
{
	ImGui::Text("ProgramNo: %u", mProgramNo);
}

void BankFile::drawUI()
{
	InnerFile::drawUI();
}

void BankFile::drawFileUI()
{
	if (ImGui::BeginChild("Instruments", ImVec2(0.0f, ImGui::GetWindowHeight() / 2.0f), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY))
	{
    	DrawAllItemsUI("Instrument", mInstrumentList);
	}
	ImGui::EndChild();

	if (ImGui::BeginChild("Keyboard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border))
	{
		static int PrevNoteActive = -1;
		//ImGui_PianoKeyboard("PianoTest", ImVec2(1024, 70), &PrevNoteActive, 0, 127, TestPianoBoardFunct, nullptr, nullptr);
		ImGui_PianoKeyboard("PianoTest", ImVec2(ImGui::GetWindowContentRegionMax().x, 70), &PrevNoteActive, 0, 127, TestPianoBoardFunct, sSelectedItem, nullptr);
	}
	ImGui::EndChild();
}

void BankFile::doRead(const void* fileAddr)
{
	nw::snd::internal::BankFileReader reader(fileAddr);
	SEAD_ASSERT(reader.IsInitialized());

	for (s32 programNo = 0; programNo < reader.GetInstrumentCount(); programNo++)
	{
		const nw::snd::internal::BankFile::Instrument* instrumentInfo = reader.GetInstrument(programNo);
		if (!instrumentInfo)
		{
			continue;
		}

		Instrument* instrument = new Instrument();
		instrument->mId = mInstrumentList.size();

		instrument->mEnableName = true;
		instrument->mName = "Instrument";

		instrument->mProgramNo = programNo;
		instrument->read(instrumentInfo, *reader.GetWaveIdTable());

		mInstrumentList.pushBack(instrument);
	}
}

u32 BankFile::doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const
{
	SEAD_ASSERT(false);
	return 0;
}

//

void ImGui_PianoKeyboard(const char* IDName, ImVec2 Size, int* PrevNoteActive, int BeginOctaveNote, int EndOctaveNote, ImGuiPianoKeyboardProc Callback, void* UserData, ImGuiPianoStyles* Style)
{
	// const
	static int NoteIsDark[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
	static int NoteLightNumber[12] = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6, 7 };
	static float NoteDarkOffset[12] = { 0.0f,  -2.0f / 3.0f, 0.0f, -1.0f / 3.0f, 0.0f, 0.0f, -2.0f / 3.0f, 0.0f, -0.5f, 0.0f, -1.0f / 3.0f, 0.0f };

	// fix range dark keys
	if (NoteIsDark[BeginOctaveNote % 12] > 0) BeginOctaveNote++;
	if (NoteIsDark[EndOctaveNote % 12] > 0) EndOctaveNote--;

	// bad range
	if (!IDName || !Callback || BeginOctaveNote < 0 || EndOctaveNote < 0 || EndOctaveNote <= BeginOctaveNote) return;

	// style
	static ImGuiPianoStyles ColorsBase;
	if (!Style) Style = &ColorsBase;

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return;
	
	const ImGuiID id = window->GetID(IDName);

	ImDrawList* draw_list = window->DrawList;

	ImVec2 Pos = window->DC.CursorPos;
	ImVec2 MousePos = ImGui::GetIO().MousePos;

	// sizes
	int CountNotesAllign7 = (EndOctaveNote / 12 - BeginOctaveNote / 12) * 7 + NoteLightNumber[EndOctaveNote % 12] - (NoteLightNumber[BeginOctaveNote % 12] - 1);

	float NoteHeight	= Size.y;
	float NoteWidth		= Size.x / (float)CountNotesAllign7;

	float NoteHeight2	= NoteHeight * Style->NoteDarkHeight;
	float NoteWidth2	= NoteWidth * Style->NoteDarkWidth;
	
	// minimal size draw
	if (NoteHeight < 5.0 || NoteWidth < 3.0) return;

	// minimal size using mouse
	bool isMouseInput = (NoteHeight >= 10.0 && NoteWidth >= 5.0);

	// item
	const ImRect bb(Pos, ImVec2(Pos.x + Size.x, Pos.y + Size.y));
	ImGui::ItemSize(Size, 0);
	if (!ImGui::ItemAdd(bb, id)) return;

	// item input
	bool held = false;
	if (isMouseInput) {
		ImGui::ButtonBehavior(bb, id, nullptr, &held, 0);
	}

	int		NoteMouseColision = -1;
	float	NoteMouseVel = 0.0f;

	float OffsetX = bb.Min.x;
	float OffsetY = bb.Min.y;
	float OffsetY2 = OffsetY + NoteHeight;
	for (int RealNum = BeginOctaveNote; RealNum <= EndOctaveNote; RealNum++) {
		int Octave	= RealNum / 12;
		int i		= RealNum % 12;

		if (NoteIsDark[i] > 0) continue;
		
		ImRect NoteRect( 
			round(OffsetX), 
			OffsetY, 
			round(OffsetX + NoteWidth), 
			OffsetY2 
		);

		if (held && NoteRect.Contains(MousePos)) {
			NoteMouseColision	= RealNum;
			NoteMouseVel		= (MousePos.y - NoteRect.Min.y) / NoteHeight;
		}

		bool isActive = Callback(UserData, NoteGetStatus, RealNum, 0.0f);
		
		draw_list->AddRectFilled(	NoteRect.Min, NoteRect.Max, Style->Colors[isActive ? 2 : 0], 0.0f);

		draw_list->AddRect(			NoteRect.Min, NoteRect.Max, Style->Colors[4], 0.0f);

		OffsetX += NoteWidth;
	}

	// draw dark notes
	OffsetX = bb.Min.x;
	OffsetY = bb.Min.y;
	OffsetY2 = OffsetY + NoteHeight2;
	for (int RealNum = BeginOctaveNote; RealNum <= EndOctaveNote; RealNum++) {
		int Octave	= RealNum / 12;
		int i		= RealNum % 12;

		if (NoteIsDark[i] == 0)  {
			OffsetX += NoteWidth;
			continue;
		}
		
		float OffsetDark = NoteDarkOffset[i] * NoteWidth2;
		ImRect NoteRect(
			round(OffsetX + OffsetDark), 
			OffsetY, 
			round(OffsetX + NoteWidth2 + OffsetDark),
			OffsetY2
		);

		if (held && NoteRect.Contains(MousePos)) {
			NoteMouseColision	= RealNum;
			NoteMouseVel		= (MousePos.y - NoteRect.Min.y) / NoteHeight2;
		}

		bool isActive = Callback(UserData, NoteGetStatus, RealNum, 0.0f);

		draw_list->AddRectFilled(	NoteRect.Min, NoteRect.Max, Style->Colors[isActive ? 3 : 1], 0.0f);

		draw_list->AddRect(			NoteRect.Min, NoteRect.Max, Style->Colors[4], 0.0f);
	}

	// mouse note click
	if (*PrevNoteActive != NoteMouseColision) {
		Callback(UserData, NoteOff, *PrevNoteActive, 0.0f);
		*PrevNoteActive = -1;

		if (held && NoteMouseColision >= 0) {
			Callback(UserData, NoteOn, NoteMouseColision, NoteMouseVel);
			*PrevNoteActive = NoteMouseColision;
		}
	}
}
