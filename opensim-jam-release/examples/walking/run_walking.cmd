REM ===================
REM Run Walking Example
REM ===================

rem Let Windows know where the plugin and opensim libraries are
set BIN=%CD%\..\..\bin
set OPENSIM=%CD%\..\..\opensim
set PATH=%BIN%;%OPENSIM%;%PATH%

REM COMAK Inverse Kinematics
REM %BIN%\comak-inverse-kinematics %BIN%\jam_plugin.dll .\inputs\comak_inverse_kinematics_settings.xml
REM move out.log results\comak-inverse-kinematics_out.log
REM move err.log results\comak-inverse-kinematics_err.log

REM REM COMAK
REM %BIN%\comak %BIN%\jam_plugin.dll .\inputs\comak_settings.xml
REM move out.log results\comak_out.log
REM move err.log results\comak_err.log

REM Joint Mechanics Analysis
REM %BIN%\joint-mechanics %BIN%\jam_plugin.dll .\inputs\joint_mechanics_settings.xml 
REM move out.log results\joint_mechanics_out.log
REM move err.log results\joint_mechanics_err.log

REM rem Inverse Dynamics
REM %OPENSIM%\opensim-cmd  -L %BIN%\jam_plugin.dll run-tool .\inputs\inverse_dynamics_settings.xml 
REM move out.log results\inverse_dynamics_out.log
REM move err.log results\inverse_dynamics_err.log

REM REM COMAK - Muscle Weights
REM %BIN%\comak %BIN%\jam_plugin.dll .\inputs\comak_settings_muscle_weights.xml
REM move out.log results\comak_muscle_weights_out.log
REM move err.log results\comak_muscle_weights_err.log

REM REM Joint Mechanics Analysis - Muscle Weights
REM %BIN%\joint-mechanics %BIN%\jam_plugin.dll .\inputs\joint_mechanics_settings_muscle_weights.xml 
REM move out.log results\joint_mechanics_muscle_weights_out.log
REM move err.log results\joint_mechanics_muscle_weights_err.log

REM COMAK - Contact Energy
%BIN%\comak %BIN%\jam_plugin.dll .\inputs\comak_settings_muscle_weights.xml
move out.log results\comak_muscle_weights_out.log
move err.log results\comak_muscle_weights_err.log

REM Joint Mechanics Analysis - Contact Energy
%BIN%\joint-mechanics %BIN%\jam_plugin.dll .\inputs\joint_mechanics_settings_muscle_weights.xml 
move out.log results\joint_mechanics_muscle_weights_out.log
move err.log results\joint_mechanics_muscle_weights_err.log

pause