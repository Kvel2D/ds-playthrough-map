#version 430 core

out vec4 color;

in vec3 normal_pass;
in vec3 frag_pos_pass;

uniform vec3 light_pos;
uniform vec3 light_color;
uniform vec4 object_color;

void main(void)
{
    vec3 ambient = 0.1 * light_color;
    
    vec3 light_dir = normalize(light_pos - frag_pos_pass);
    float diff = max(dot(normal_pass, light_dir), 0.5);
    vec3 diffuse = diff * light_color;
            
    vec3 result = (ambient + diffuse) * vec3(object_color.xyz);
    color = vec4(result, object_color.w);
}