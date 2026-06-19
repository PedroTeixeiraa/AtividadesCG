# Jeep Renegade — Visualizador Final 3D

Visualizador unificado (Computacao Grafica / Unisinos) que integra os requisitos
minimos da disciplina em uma cena simples: carregamento de malhas `.obj`,
materiais `.mtl`, textura, iluminacao de Phong com 3 luzes, camera FPS,
selecao/transformacao de objetos e animacao por curva de Bezier.

## Como compilar e rodar

A partir da raiz do repositorio:

```bash
cmake -S grauB -B grauB/build
cmake --build grauB/build --target JeepRenegade

cd grauB/build
./JeepRenegade
```

> **Importante:** execute de dentro de `grauB/build/`. Os caminhos dos assets
> foram configurados considerando essa pasta como diretorio de execucao.

## Controles

| Tecla | Acao |
|-------|------|
| `W A S D` + mouse | Navegar com a camera FPS |
| Scroll | Zoom / campo de visao |
| `TAB` | Selecionar o proximo objeto |
| Setas | Transladar o objeto selecionado no plano XZ |
| `I` / `J` | Subir/descer o objeto selecionado |
| `X` `Y` `Z` | Ativar rotacao automatica no eixo escolhido |
| `[` / `]` | Diminuir/aumentar escala uniforme |
| `T` | Ligar/desligar textura |
| `M` | Mostrar cor do material `.mtl` sem textura |
| `1` `2` `3` | Ligar/desligar cada luz |
| `P` | Play/pause da trajetoria de Bezier |
| `ESC` | Sair |

## Cena (`grauB/assets/cena.txt`)

Arquivo de configuracao lido em tempo de execucao. Formato usado:

```text
camera x y z
light  px py pz  r g b  intensidade  ligado
mesh   nome caminho_do_obj
object nome mesh  x y z  rotX rotY rotZ  escala  [bezier pontos...]
```

Exemplo de objeto com trajetoria:

```text
object jeep_animado jeep 0.0 -0.25 0.0 0.0 18.0 0.0 0.72 bezier ...
```

## Onde esta cada calculo (guia da arguicao)

| Item da rubrica | Arquivo / funcao |
|-----------------|------------------|
| Parser do arquivo de cena | `src/JeepRenegade.cpp` -> `loadSceneConfig()` |
| Parser do material `.mtl` (Ka/Kd/Ks/Ns) | `src/JeepRenegade.cpp` -> `loadPhongMaterialsFromMTL()` |
| Carregamento de malha `.obj` | `src/JeepRenegade.cpp` -> `loadSimpleOBJ()` |
| Passagem de uniforms | `src/JeepRenegade.cpp` -> loop principal do `main()` |
| Matriz de **Model** (T * R * S) | `src/JeepRenegade.cpp` -> bloco "Matriz Model" no `main()` |
| Matriz de **View** | `src/JeepRenegade.cpp` -> `Camera::getViewMatrix()` |
| Matriz de **Projecao** | `src/JeepRenegade.cpp` -> `glm::perspective()` no loop |
| Iluminacao de Phong | `src/JeepRenegade.cpp` -> `fragmentShaderSource` |
| Curva de Bezier | `src/JeepRenegade.cpp` -> `cubicBezier()` / `updateTrajectory()` |

## Objetos da cena

- **Jeep Renegade** — modelo `.obj` principal, com material `.mtl`, textura e trajetoria animada.
- **Suzanne** — malha `.obj` auxiliar para demonstrar mais de um objeto na cena.
- **Suzanne subdividida** — segunda malha `.obj` auxiliar para composicao simples.

## Assets e referencias

Assets usados:

- `../assets/Modelos3D/Jeep_Renegade_2016.obj`
- `../assets/Modelos3D/Jeep_Renegade_2016.mtl`
- `../assets/tex/renegade-tex.jpg`
- `../assets/Modelos3D/Suzanne.obj`
- `../assets/Modelos3D/SuzanneSubdiv1.obj`
- `https://www.cgtrader.com/items/1017599/download-page`

Referencias:

- LearnOpenGL: https://learnopengl.com/
- OpenGL/Khronos: https://www.khronos.org/opengl/
- GLFW: https://www.glfw.org/documentation.html
- GLM: https://github.com/g-truc/glm
- stb_image: https://github.com/nothings/stb
