-- example script for light effects on IIDX TT stab movement
-- create a folder called "script" next to spice and put me in there
--------------------------------------------------------------------------------

-- settings
tt_duration = 0.25
zero_duration = 0.3141592
loop_delta = 1 / 240
curve_pow = 1 / 4
col_r = 1.0
col_g = 0.0
col_b = 0.0
light_p1_r = "Side Panel Left Avg R"
light_p1_g = "Side Panel Left Avg G"
light_p1_b = "Side Panel Left Avg B"
light_p2_r = "Side Panel Right Avg R"
light_p2_g = "Side Panel Right Avg G"
light_p2_b = "Side Panel Right Avg B"

-- wait for game
while not analogs.read()["Turntable P1"] do yield() end

-- initial state
tt1_last = tonumber(analogs.read()["Turntable P1"].state)
tt2_last = tonumber(analogs.read()["Turntable P2"].state)
tt1_diff_last = 0
tt2_diff_last = 0
tt1_trigger = 0
tt2_trigger = 0
tt1_zero_elapsed = 0
tt2_zero_elapsed = 0

-- main loop
while true do

    -- read state
    tt1 = tonumber(analogs.read()["Turntable P1"].state)
    tt2 = tonumber(analogs.read()["Turntable P2"].state)
    time_cur = time()

    -- calculate difference
    tt1_diff = tt1 - tt1_last
    tt2_diff = tt2 - tt2_last

    -- fix wrap around
    if math.abs(tt1_diff) > 0.5 then tt1_diff = 0 end
    if math.abs(tt2_diff) > 0.5 then tt2_diff = 0 end

    -- trigger on movement start and direction changes
    if (tt1_diff_last == 0 and tt1_diff ~= 0)
        or (tt1_diff_last > 0 and tt1_diff < 0)
        or (tt1_diff_last < 0 and tt1_diff > 0) then
        tt1_trigger = time_cur
    end
    if (tt2_diff_last == 0 and tt2_diff ~= 0)
        or (tt2_diff_last > 0 and tt2_diff < 0)
        or (tt2_diff_last < 0 and tt2_diff > 0) then
        tt2_trigger = time_cur
    end

    -- light effects when last trigger is still active
    if time_cur - tt1_trigger < tt_duration then
        brightness = 1 - ((time_cur - tt1_trigger) / tt_duration) ^ curve_pow
        lights.write({[light_p1_r]={state=brightness*col_r}})
        lights.write({[light_p1_g]={state=brightness*col_g}})
        lights.write({[light_p1_b]={state=brightness*col_b}})
    else
        lights.write_reset(light_p1_r)
        lights.write_reset(light_p1_g)
        lights.write_reset(light_p1_b)
    end
    if time_cur - tt2_trigger < tt_duration then
        brightness = 1 - ((time_cur - tt2_trigger) / tt_duration) ^ curve_pow
        lights.write({[light_p2_r]={state=brightness*col_r}})
        lights.write({[light_p2_g]={state=brightness*col_g}})
        lights.write({[light_p2_b]={state=brightness*col_b}})
    else
        lights.write_reset(light_p2_r)
        lights.write_reset(light_p2_g)
        lights.write_reset(light_p2_b)
    end

    -- flush HID light output
    lights.update()

    -- turntable movement detection
    -- doesn't set the diff back to zero unless enough time has passed
    if tt1_diff == 0 then
        tt1_zero_elapsed = tt1_zero_elapsed + loop_delta
        if tt1_zero_elapsed >= zero_duration then
            tt1_diff_last = tt1_diff
        end
    else
        tt1_zero_elapsed = 0
        tt1_diff_last = tt1_diff
    end
    if tt2_diff == 0 then
        tt2_zero_elapsed = tt2_zero_elapsed + loop_delta
        if tt2_zero_elapsed >= zero_duration then
            tt2_diff_last = tt2_diff
        end
    else
        tt2_zero_elapsed = 0
        tt2_diff_last = tt2_diff
    end

    -- remember state
    tt1_last = tt1
    tt2_last = tt2

    -- loop end
    sleep(loop_delta)
end
