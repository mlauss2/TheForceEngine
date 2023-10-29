#include "levelEditorHistory.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <assert.h>
#include <algorithm>

using namespace TFE_Editor;

namespace LevelEditor
{
	enum LevCommand
	{
		LCmd_MoveVertex = CMD_START,
		LCmd_SetVertex,
		LCmd_MoveFlat,
		LCmd_Count
	};
		
	// Command Functions
	void cmd_applyMoveVertices();
	void cmd_applySetVertex();
	void cmd_applyMoveFlats();

	void levHistory_init()
	{
		history_init(level_unpackSnapshot, level_createSnapshot);

		history_registerCommand(LCmd_MoveVertex, cmd_applyMoveVertices);
		history_registerCommand(LCmd_SetVertex, cmd_applySetVertex);
		history_registerCommand(LCmd_MoveFlat, cmd_applyMoveFlats);
		history_registerName(LName_MoveVertex, "Move Vertice(s)");
		history_registerName(LName_SetVertex, "Set Vertex Position");
		history_registerName(LName_MoveWall, "Move Wall(s)");
		history_registerName(LName_MoveFlat, "Move Flat(s)");
	}

	void levHistory_destroy()
	{
		history_destroy();
	}

	void levHistory_clear()
	{
		history_clear();
	}

	void levHistory_createSnapshot(const char* name)
	{
		history_createSnapshot(name);
	}
		
	void levHistory_undo()
	{
		// TODO: Figure out how to track sector changes properly.
		history_step(-1);
	}

	void levHistory_redo()
	{
		// TODO: Figure out how to track sector changes properly.
		history_step(1);
	}

	////////////////////////////////
	// Commands
	////////////////////////////////
	// cmd_add*   - called from the level editor to add the command to the history.
	// cmd_apply* - called from the history/undo/redo system to apply the command from the history.
	
	/////////////////////////////////
	// Move Vertices
	void cmd_addMoveVertices(s32 count, const FeatureId* vertices, Vec2f delta, LevCommandName name)
	{
		// Try to create a command, if it returns false - then a snapshot was created
		// instead.
		if (!history_createCommand(LCmd_MoveVertex, name)) { return; }
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addArrayU64(count, vertices);
		hBuffer_addVec2f(delta);
	}

	void cmd_applyMoveVertices()
	{
		// Extract the command data.
		const s32 count = hBuffer_getS32();
		const FeatureId* vertices = (FeatureId*)hBuffer_getArrayU64(count);
		const Vec2f delta = hBuffer_getVec2f();
		// Call the editor command.
		edit_moveVertices(count, vertices, delta);
	}

	/////////////////////////////////
	// Set Vertex
	void cmd_addSetVertex(FeatureId vertex, Vec2f pos)
	{
		// Try to create a command, if it returns false - then a snapshot was created
		// instead.
		if (!history_createCommand(LCmd_SetVertex, LName_SetVertex)) { return; }
		// Add the command data.
		hBuffer_addU64(vertex);
		hBuffer_addVec2f(pos);
	}

	void cmd_applySetVertex()
	{
		// Extract the command data.
		const FeatureId id = (FeatureId)hBuffer_getU64();
		const Vec2f pos = hBuffer_getVec2f();
		// Call the editor command.
		edit_setVertexPos(id, pos);
	}

	/////////////////////////////////
	// Move Flats
	void cmd_addMoveFlats(s32 count, const FeatureId* flats, f32 delta)
	{
		// Try to create a command, if it returns false - then a snapshot was created
		// instead.
		if (!history_createCommand(LCmd_MoveFlat, LName_MoveFlat)) { return; }
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addArrayU64(count, flats);
		hBuffer_addF32(delta);
	}

	void cmd_applyMoveFlats()
	{
		// Extract the command data.
		const s32 count = hBuffer_getS32();
		const FeatureId* flats = (FeatureId*)hBuffer_getArrayU64(count);
		const f32 delta = hBuffer_getF32();
		// Call the editor command.
		edit_moveFlats(count, flats, delta);
	}
}