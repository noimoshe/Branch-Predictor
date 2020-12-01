# Branch Predictor
The following code implements a 2-Level Branch Predictor. <br /> 

# Main Functions
Includes 4 main functions:<br /><br />
1.**BP_init** <br />
Initializes the predictor.<br /><br />
2.**BP_predict** <br />
Gets current pc. If pc is a branch command, returns jump prediction - true for Taken, false for Not Taken. <br />
In case of Teken prediction, dst parameter will contain the target pc (which was calculated).<br />
In case of Not Taken or a non-branch command, dst will contain pc+4.<br /><br />
3.**BP_update**<br />
Updates predictor's state according to last prediction.
4.**BP_GetStats**<br />

# Fsm for prediction:
<img src="https://github.com/noimoshe/Branch-Predictor/blob/main/fsmStates.JPG">
