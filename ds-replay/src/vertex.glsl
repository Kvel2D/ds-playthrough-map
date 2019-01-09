#version 430 core

out vec3 normal_pass;
out vec3 frag_pos_pass;

in vec3 position_in;
in vec3 normal_in;
in vec3 offset_in;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main(void)
{
    frag_pos_pass = vec3(model * vec4(position_in, 1.0)) + offset_in;
    normal_pass = normal_in;  
    
    gl_Position = projection * view * vec4(frag_pos_pass, 1.0);
}