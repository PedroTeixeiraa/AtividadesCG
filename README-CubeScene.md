# CubeScene

Exercício `CubeScene` em OpenGL com:
- cubo colorido por face (malha triangular),
- rotação nos eixos X/Y/Z,
- translação e escala uniforme por teclado,
- múltiplas instâncias de cubo,
- leitura de malha 3D `.obj` com normais de vértice (`vn`),
- leitura de material `.mtl` com coeficientes de iluminação (`Ka`, `Kd`, `Ks`, `Ns`) e textura difusa (`map_Kd`),
- iluminação por pixel com modelo de Phong no fragment shader.

## Leitor de malha 3D (OBJ + MTL)

- Caminho inicial configurado no código: `../assets/Modelos3D/Car.obj`.
- O carregador:
  - lê linhas `v`, `vt`, `vn`, `f`, `mtllib` e `usemtl`,
  - aceita faces `f v`, `f v/vt/vn` e `f v//vn`,
  - triangula faces com mais de 3 vértices usando fan triangulation,
  - centraliza e normaliza escala da malha automaticamente para caber no enquadramento padrão da cena,
  - armazena posição + UV + normal + cor no buffer de vértices.
- No `.mtl`, são lidos:
  - `newmtl`
  - `Ka`, `Kd`, `Ks`, `Ns`
  - `map_Kd` (textura difusa).
- O shader calcula ambiente, difusa e especular (Phong), combinando material e textura quando disponível.
- A luz principal está fixa à direita da cena para destacar visualmente as componentes difusa e especular do Phong.
- Se houver falha de leitura/parsing de `.obj`, `.mtl` ou textura, o `CubeScene` faz fallback para o cubo hardcoded colorido.

## Limitações da etapa

- Em arquivos com múltiplos materiais/texturas, o render separa lotes por `usemtl` e aplica um conjunto de coeficientes por lote.
- O shader usa `Kd` do material como cor base quando não há textura válida para o lote.
- Quando um coeficiente está ausente no `.mtl`, são usados defaults:
  - `Ka = 0.1`
  - `Kd = 1.0`
  - `Ks = 0.5`
  - `Ns = 32`

## Pré-requisitos

- GLAD configurada no projeto (`include/glad` e `common/glad.c`)
- CMake instalado
- GLFW disponível no sistema
  - macOS (Homebrew): `brew install glfw`

## Comandos necessários

Na raiz do projeto:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Executar o CubeScene

No diretório `build`:

```bash
./CubeScene
```

No Windows (PowerShell/CMD):

```bat
CubeScene.exe
```

## Controles do teclado

- `X`, `Y`, `Z`: seleciona rotação do cubo ativo no respectivo eixo
- `W`, `S`: move no eixo Z (frente/trás)
- `A`, `D`: move no eixo X (esquerda/direita)
- `I`, `J`: move no eixo Y (cima/baixo)
- `[` e `]`: escala uniforme (diminuir/aumentar)
- `N`: cria novo cubo
- `TAB`: alterna cubo ativo
- `ESC`: fecha a janela
