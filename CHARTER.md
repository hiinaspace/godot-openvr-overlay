Idea is to make OpenVR projective overlays buildable in Godot, i.e. 3d scenes that are composited on top
of a main openvr app, aka "AR in VR".

MVP is a simple godot scene with a few primitives using the standard shader, in opengl/compatability renderer,
being rendered as a projective overlay, with the godot cameras updated to match the VR headset eye poses, and the
skybox/background being transparent. I think the abstraction that'd work is like a OpenVRViewport node, but I don't
know the godot architecture well enough to say.

If that generally works, can add a similar abstraction to the
XROrigin/XRController/XRCamera3d for accessing those nodes from the code and
adding attached things. Don't need to strictly be compatible with the whole XR
subsystem of godot though. Might also add an abstraction for making additional
quad overlays that are attached to world or controllers (where the compositor
does the final rendering in the correct position so they're more stable than a projective overlay).

Can look at S:\code\openvr-overlay-bunny and S:\code\oversaber as examples of how the api works.
Also S:\lib\openvr and S:\lib\godot for reference.
