import dicp.vendor.TopsGraph.config as topsconfig
topsconfig.remove_unused_dispatchkey()


def topsgraph(gm, fake_input_tensor):
    from dicp.dynamo_bridge.compile_fx import compile_fx

    return compile_fx(gm, fake_input_tensor, "topsgraph")
