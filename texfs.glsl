#version 330 core


uniform bool useEmissionTexture;

uniform sampler2D emissionTexture;

vec3 ambient = vec3(0.1f);
vec3 diffuse = vec3(1.0f);
vec3 specular = vec3(1.0f);
vec3 emission  = vec3(0.1f);
float shininess = 3.0f;

vec3 Lambient = vec3(0.1f);
vec3 Ldiffuse = vec3(1.0f);
vec3 Lspecular = vec3(1.0f);
vec3 Lposition = vec3(0, 100, 0);

out vec4 fragmentColor;

smooth in vec3 o_position;
smooth in vec2 o_texCoords;
smooth in vec3 o_normal;

uniform mat4 pvmMatrix;
uniform mat4 vmMatrix;
uniform mat4 mMatrix;
uniform mat4 vMatrix;
// transpose(inverrse(mMatrix))
uniform mat3 nMatrix;

void main() {
    if (useEmissionTexture) {
        vec3 positionOfLight = (vMatrix * vec4(Lposition, 1.0f)).xyz;

        vec3 L = normalize(positionOfLight - o_position);
        float NdotL = max(0.0, dot(o_normal, L));
#if 1
        for (int i = 0; i < 400000; i++)
            NdotL = max(1.0, NdotL * length(sinh(log(o_position))));
#endif

        fragmentColor = vec4((texture(emissionTexture, o_texCoords)).xyz, 1.0f) * NdotL;

    }
    else {

        vec3 final = vec3(0.0);

        vec3 positionOfLight = (vMatrix * vec4(Lposition, 1.0f)).xyz;

        vec3 L = normalize(positionOfLight - o_position);
        vec3 R = reflect(-L, o_normal);
        vec3 V = normalize(-o_position);

        float RdotV = max(0.0, dot(R, V));
        float NdotL = max(0.0, dot(o_normal, L));

#if 1
        for (int i = 0; i < 50000; i++)
            NdotL = max(1.0, NdotL * length(sinh(log(o_position))));
#endif

        final += ambient * Lambient;
        
        final += diffuse * Ldiffuse * NdotL;        

        if (shininess == 0) {
            final += specular * Lspecular;
        }
        else {
            final += specular * Lspecular * pow(RdotV, shininess);
        }

        fragmentColor =  vec4(final, 1.0);
    }
}
