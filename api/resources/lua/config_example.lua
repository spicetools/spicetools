-- script examples
-- no proper documentation yet
-- create a folder called "script" next to spice and put me in there
-- then open the config and if needed select IIDX for the demo
--------------------------------------------------------------------------------

-- sleep for 0.2 seconds
sleep(0.2)

-- log functions
log_misc("example misc")
log_info("example info")
log_warning("example warning")
--log_fatal("this would terminate")

-- print time
log_info(time())

-- show message box
msgbox("You are running the example script! Select IIDX if not already done.")

-- wait until analog is available
while not analogs.read()["Turntable P1"] do yield() end

-- write button state
buttons.write({["P1 Start"]={state=1}})

-- write analog state
analogs.write({["Turntable P1"]={state=0.33}})

-- write light state
lights.write({["P2 Start"]={state=0.8}})

-- import other libraries in "script" folder
--local example = require('script.example')

-- demo
while true do

    -- analog animation
    analogs.write({["Turntable P2"]={state=math.abs(math.sin(time()))}})

    -- button blink
    if math.cos(time() * 10) > 0 then
        buttons.write({["P1 1"]={state=1}})
    else
        buttons.write({["P1 1"]={state=0}})
    end

    -- flush HID light output
    lights.update()

    -- check for keyboard press
    if GetAsyncKeyState(0x20) > 0 then
        msgbox("You pressed space!")
    end
end
