import win32gui, win32api, win32con, win32process
import string, random, time
from memory.api import Memopy
from utils import GetRenderViewFromLog

Window = None
Process = Memopy(0)

name_offset = 0x1
children_offset = 0x1

def fetch_roblox_pid():
    global Window, Process

    Window = win32gui.FindWindow(None, "Roblox")
    ProcessId = win32process.GetWindowThreadProcessId(Window)[1]

    return ProcessId

def initialize():
    ProcessId = fetch_roblox_pid()
    Process.update_pid(ProcessId)

    if not Process.process_handle:
        return False, -1
    
    return True, ProcessId

def findFirstChild(instance, name):
    global name_offset
    global children_offset
    container = []
    while True:
        start = Process.read_longlong(instance + children_offset)
        end = Process.read_longlong(start + 0x8) # as far as i know the size never changes but im not sure
        instances = Process.read_longlong(start)
        f = 0

        while f != 30:
            container.append(Process.read_longlong(instances))
            instances += 16
            f += 1

        for maybe_child_of_instance in container:
            name_ptr = Process.read_longlong(maybe_child_of_instance + name_offset)
            name_strrrrrrrrrrrrrr = Process.read_string(name_ptr)

            if(name_strrrrrrrrrrrrrr == name):
                print("[+] Children: " + hex(children_offset))
                return maybe_child_of_instance

        children_offset += 1
        container.clear()

def main():
    print("[+] Finding Roblox...")
    success, pid = initialize()
    if success:
        print("[+] Found Roblox: "+str(pid))

        time.sleep(1)

        print("[+] Getting DataModel...")
       # Process.suspend()

        RenderView = GetRenderViewFromLog()

        datamodel_ptr = Process.read_longlong(RenderView + 0x118)
        datamodel = Process.read_longlong(datamodel_ptr + 0x1a8)

        print("[+] Found DataModel: " + hex(datamodel))

       # Process.resume()
        time.sleep(1)

        print("[+] Dumping...")

        time.sleep(2)

        global name_offset
        workspace_offset = 0x1
        parent_offset = 0x1
        global children_offset
        class_descriptor_offset = 0x1
        players_address = 0x1
        localplayer_offset = 0x1
        modelinstance_offset = 0x1
        placeid_offset = 0x1
        value_offset = 0x1
        jobid_offset = 0x1

        # CONFIG
        REAL_PLAYER_NAME = "JJSploitWorksV2" # W Name
        PLACE_ID = 109337229657596

        while True:

            ptr = Process.read_longlong(datamodel + name_offset)
            instance_name = Process.read_string(ptr)

            if instance_name == "Game":
                print("[+] Name: " + hex(name_offset))
                break

            if name_offset == 0x48:
                print(instance_name)

            name_offset += 1

        while True:
            placeid = Process.read_longlong(datamodel + placeid_offset)

            if placeid == PLACE_ID:
                print("[+] PlaceId: " + hex(placeid_offset))
                print("[+] GameId: " + hex(placeid_offset - 0x8))
                break

            placeid_offset += 1


        # need workspace for the parent offset yea
        while True:
            workspace_ptr = Process.read_longlong(datamodel + workspace_offset)

            name_ptrr = Process.read_longlong(workspace_ptr + name_offset)

            name_string = Process.read_string(name_ptrr)

            if name_string == "Workspace":
                print("[+] Workspace: " + hex(workspace_offset))
                break
        
            workspace_offset += 1

        
        while True:
            parent_ptr = Process.read_longlong(workspace_ptr + parent_offset)

            name_ptrrr = Process.read_longlong(parent_ptr + name_offset)

            name_str = Process.read_string(name_ptrrr)

            if name_str == "Game":
                print("[+] Parent: " + hex(parent_offset))
                break

            parent_offset += 1

        while True:
            class_descriptor_ptr = Process.read_longlong(datamodel + class_descriptor_offset)

            classname_ptr = Process.read_longlong(class_descriptor_ptr + 0x8)

            classname = Process.read_string(classname_ptr)

            if classname == "DataModel":
                print("[+] Class Descriptor: " + hex(class_descriptor_offset))
                break

            class_descriptor_offset += 1

        Process.suspend()
        players_address = findFirstChild(datamodel, "Players")
        Process.resume()

        Process.suspend()
        while True:
            localplayer_ptr = Process.read_longlong(players_address + localplayer_offset)
            name_sigma = Process.read_longlong(localplayer_ptr + name_offset)

            if localplayer_offset > 0x1000:
                print("[-] LocalPlayer: failed")
                break

            if Process.read_string(name_sigma) == REAL_PLAYER_NAME:
                print("[+] LocalPlayer: " + hex(localplayer_offset))

                # Finds value offset
                stringInstance_ptr = findFirstChild(localplayer_ptr, "StrValInst")
                while True:
                    if value_offset > 0x1000:
                        print("[-] Value: failed")
                        break
                    if Process.read_string(stringInstance_ptr + value_offset) == "Hallo":
                        print("[+] Value: " + hex(value_offset))
                        break
                    value_offset += 1

                break

            localplayer_offset+=1
        Process.resume()

        Process.suspend()
        while True:
            found = False
            possible_char = Process.read_longlong(localplayer_ptr + modelinstance_offset)
            possible_char_class_descriptor = Process.read_longlong(possible_char + class_descriptor_offset)

            classname_offset = 0x1
            while True:
                possible_char_class_descriptor_name = Process.read_string(Process.read_longlong(possible_char_class_descriptor + classname_offset))

                if possible_char_class_descriptor_name == "Model":
                    found = True
                    break

                if classname_offset > 0xFF:
                    classname_offset = 0
                    break

                classname_offset += 1

            if found:
                print("[+] ModelInstance: " + hex(modelinstance_offset))
                print("[+] ClassName: " + hex(classname_offset))
                break

            if modelinstance_offset > 0x1000:
                print("[-] ModelInstance: failed")
                print("[-] ClassName: failed")
                break

            modelinstance_offset += 1
        Process.resume()
        
    else:
        print("[+] Couldn't find Roblox")
    

if __name__ == "__main__":
    main()
