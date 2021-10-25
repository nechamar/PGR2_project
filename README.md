In the application, there are used a few techniques, which transfer vertex and texturing data from main memory to GPU memory asynchronously with respect to rendering. In the project, OpenGL is used with multiple threads on CPU. Techniques used are: Asynchronous texture transfer during rendering of other parts of the scene, asynchronous mapping of GPU memory, loading of vertex data for the following frame during rendering of current frame using one or multiple OpenGL contexts.
Texture transfer: 
	textures without asynchronous transfer: 0.0067 s for a texture,
	textures with asynchronous transfer: 0.0039 s for a texture.
Vertex data transfer: 
	Synchronized method: 0.122 s per frame,
	Asynchronous memory mapping: 0.099 s per frame,	
	Loading for following frame with one context: 0.088 s per frame,
	Loading for following frame with more contexts: 0.088 s per frame.