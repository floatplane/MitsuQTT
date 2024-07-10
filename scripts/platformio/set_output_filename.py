Import("env")

def get_cpp_defines():
    build_flags = env.ParseFlags(env['BUILD_FLAGS'])
    build_defines = dict()    
    for x in build_flags.get("CPPDEFINES"):
        if type(x) is tuple:
            (k,v) = x
            build_defines[k] = v
        elif type(x) is list:
            k = x[0]
            v = x[1]
            build_defines[k] = v
        else:
            build_defines[x] = "" # empty value
    return build_defines

if not env['PIOENV'].startswith("test"):
    cpp_defines = get_cpp_defines()
    using_spiffs = 'SPIFFS' in env['PIOENV']
    # print('Using SPIFFS: ' + ('YES' if using_spiffs else 'NO'))
    filesystem = '' if using_spiffs else '-LittleFS'
    progname = ("mitsuqtt-%s%s-%s-%s" % (
        env['PIOENV'], 
        filesystem, 
        cpp_defines['MITSUQTT_BUILD_DATE'].strip('\"'), 
        cpp_defines['MITSUQTT_GIT_COMMIT'].strip('\"')
        )
    )

    env.Replace(PROGNAME=progname)
    env.Append(CPPDEFINES=[
        ('MITSUQTT_PROGNAME', '\\"%s\\"' % progname)
    ])
